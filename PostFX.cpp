#include "PostFX.h"

#include <stdio.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct PostFXColourCB
{
	XMFLOAT4 BlurColor;
};

struct PostFXBlitCB
{
	XMFLOAT4 Color;
	XMFLOAT2 UVOffset;
	XMFLOAT2 Pad;
};

const char* PostFX::ModeName(Mode mode)
{
	switch (mode) {
	case MODE_OFF: return "OFF";
	case MODE_COLOUR_FILTER: return "NORMAL (colour filter)";
	case MODE_MOTION_BLUR: return "NORMAL (motion blur)";
	default: return "?";
	}
}

HRESULT PostFX::CreateTargets(DXRender* render)
{
	ReleaseTargets();

	m_width = render->GetBackBufferWidth();
	m_height = render->GetBackBufferHeight();

	ID3D11Texture2D* backBuf = render->GetBackBufferTexture();
	if (!backBuf)
		return E_FAIL;

	D3D11_TEXTURE2D_DESC desc;
	backBuf->GetDesc(&desc);
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = 0;
	desc.CPUAccessFlags = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &m_backTex);
	if (FAILED(hr))
		return hr;

	hr = device->CreateTexture2D(&desc, nullptr, &m_frontTex);
	if (FAILED(hr))
		return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(m_backTex, &srvDesc, &m_backSRV);
	if (FAILED(hr))
		return hr;
	hr = device->CreateShaderResourceView(m_frontTex, &srvDesc, &m_frontSRV);
	return hr;
}

void PostFX::ReleaseTargets()
{
	if (m_frontSRV) { m_frontSRV->Release(); m_frontSRV = nullptr; }
	if (m_frontTex) { m_frontTex->Release(); m_frontTex = nullptr; }
	if (m_backSRV) { m_backSRV->Release(); m_backSRV = nullptr; }
	if (m_backTex) { m_backTex->Release(); m_backTex = nullptr; }
}

HRESULT PostFX::Init(DXRender* render)
{
	m_vs = nullptr;
	m_psColour = nullptr;
	m_psBlit = nullptr;
	m_cbColour = nullptr;
	m_cbBlit = nullptr;
	m_pointSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthDisabled = nullptr;
	m_blendOpaque = nullptr;
	m_blendAlpha = nullptr;
	m_blendAdditive = nullptr;
	m_backTex = nullptr;
	m_backSRV = nullptr;
	m_frontTex = nullptr;
	m_frontSRV = nullptr;
	m_width = m_height = 0;
	m_mode = MODE_MOTION_BLUR;
	m_justInitialised = true;
	m_blurR = DEFAULT_R;
	m_blurG = DEFAULT_G;
	m_blurB = DEFAULT_B;
	m_blurAlpha = DEFAULT_BLUR_ALPHA;
	m_intensity = INTENSITY;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;
	ID3DBlob* blob = nullptr;

	hr = D3DReadFileToBlob(L"ssao_vs.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: PostFX cannot read ssao_vs.cso\n");
		return hr;
	}
	hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_vs);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"colourfilter_vc_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: PostFX cannot read colourfilter_vc_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psColour);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"postfx_blit_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: PostFX cannot read postfx_blit_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psBlit);
	blob->Release();
	if (FAILED(hr))
		return hr;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	cbd.ByteWidth = sizeof(PostFXColourCB);
	hr = device->CreateBuffer(&cbd, nullptr, &m_cbColour);
	if (FAILED(hr))
		return hr;

	cbd.ByteWidth = sizeof(PostFXBlitCB);
	hr = device->CreateBuffer(&cbd, nullptr, &m_cbBlit);
	if (FAILED(hr))
		return hr;

	D3D11_SAMPLER_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sd, &m_pointSampler);
	if (FAILED(hr))
		return hr;

	D3D11_RASTERIZER_DESC rd;
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	hr = device->CreateRasterizerState(&rd, &m_rasterizer);
	if (FAILED(hr))
		return hr;

	D3D11_DEPTH_STENCIL_DESC dd;
	ZeroMemory(&dd, sizeof(dd));
	dd.DepthEnable = FALSE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
	hr = device->CreateDepthStencilState(&dd, &m_depthDisabled);
	if (FAILED(hr))
		return hr;

	D3D11_BLEND_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = FALSE;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blendOpaque);
	if (FAILED(hr))
		return hr;

	/* SRCALPHA / INVSRCALPHA — soft trail tint. */
	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blendAlpha);
	if (FAILED(hr))
		return hr;

	/* ONE / ONE — additive colour bloom (re3 RenderOverlayBlur). */
	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blendAdditive);
	if (FAILED(hr))
		return hr;

	hr = CreateTargets(render);
	if (FAILED(hr)) {
		printf("Error: PostFX CreateTargets failed\n");
		return hr;
	}

	printf("[Info] PostFX ready (%ux%u) — F9 cycles modes\n", m_width, m_height);
	return S_OK;
}

void PostFX::Cleanup()
{
	ReleaseTargets();
	if (m_blendAdditive) { m_blendAdditive->Release(); m_blendAdditive = nullptr; }
	if (m_blendAlpha) { m_blendAlpha->Release(); m_blendAlpha = nullptr; }
	if (m_blendOpaque) { m_blendOpaque->Release(); m_blendOpaque = nullptr; }
	if (m_depthDisabled) { m_depthDisabled->Release(); m_depthDisabled = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_pointSampler) { m_pointSampler->Release(); m_pointSampler = nullptr; }
	if (m_cbBlit) { m_cbBlit->Release(); m_cbBlit = nullptr; }
	if (m_cbColour) { m_cbColour->Release(); m_cbColour = nullptr; }
	if (m_psBlit) { m_psBlit->Release(); m_psBlit = nullptr; }
	if (m_psColour) { m_psColour->Release(); m_psColour = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}

void PostFX::SetBlurColor(float r, float g, float b)
{
	m_blurR = r;
	m_blurG = g;
	m_blurB = b;
}

void PostFX::SetIntensity(float intensity)
{
	m_intensity = intensity;
}

void PostFX::SetBlurAlpha(float alpha)
{
	m_blurAlpha = alpha;
}

void PostFX::SetMode(Mode mode)
{
	m_mode = mode;
	if (m_mode == MODE_MOTION_BLUR)
		m_justInitialised = true;
}

PostFX::Mode PostFX::CycleMode()
{
	m_mode = (Mode)((m_mode + 1) % MODE_COUNT);
	if (m_mode == MODE_MOTION_BLUR)
		m_justInitialised = true;
	return m_mode;
}

void PostFX::DrawFullscreen(ID3D11DeviceContext* ctx)
{
	ctx->IASetInputLayout(nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = 0, offset = 0;
	ID3D11Buffer* nullVB = nullptr;
	ctx->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);
	ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	ctx->VSSetShader(m_vs, nullptr, 0);
	ctx->Draw(3, 0);
}

void PostFX::ApplyColourFilter(DXRender* render, ID3D11Texture2D* backBuf)
{
	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	ctx->CopyResource(m_backTex, backBuf);

	PostFXColourCB cb;
	float f = m_intensity;
	cb.BlurColor = XMFLOAT4(
		m_blurR * f / 255.0f,
		m_blurG * f / 255.0f,
		m_blurB * f / 255.0f,
		30.0f / 255.0f);
	ctx->UpdateSubresource(m_cbColour, 0, nullptr, &cb, 0, 0);

	render->BindColorTargetOnly();
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthDisabled, 0);
	float blendFactor[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(m_blendOpaque, blendFactor, 0xffffffff);

	ctx->PSSetShader(m_psColour, nullptr, 0);
	ctx->PSSetConstantBuffers(0, 1, &m_cbColour);
	ctx->PSSetShaderResources(0, 1, &m_backSRV);
	ctx->PSSetSamplers(0, 1, &m_pointSampler);
	DrawFullscreen(ctx);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);
}

void PostFX::ApplyMotionBlur(DXRender* render, ID3D11Texture2D* backBuf)
{
	ID3D11DeviceContext* ctx = render->GetDeviceContext();
	float blendFactor[4] = { 0, 0, 0, 0 };

	/* Same Intensity scale as colour filter so both modes match in punch. */
	float f = m_intensity;
	float r = m_blurR * f / 255.0f;
	float g = m_blurG * f / 255.0f;
	float b = m_blurB * f / 255.0f;
	float a = m_blurAlpha * f / 255.0f;
	/* ~2px smear like re3 Vertex2. */
	XMFLOAT2 offsetUV(2.0f / (float)m_width, 2.0f / (float)m_height);

	if (!m_justInitialised) {
		render->BindColorTargetOnly();
		ctx->RSSetState(m_rasterizer);
		ctx->OMSetDepthStencilState(m_depthDisabled, 0);
		ctx->PSSetShader(m_psBlit, nullptr, 0);
		ctx->PSSetConstantBuffers(0, 1, &m_cbBlit);
		ctx->PSSetShaderResources(0, 1, &m_frontSRV);
		ctx->PSSetSamplers(0, 1, &m_pointSampler);

		PostFXBlitCB blit;

		/* Pass 1: alpha blend of offset previous frame, color*2, a=30. */
		blit.Color = XMFLOAT4(
			(r * 2.0f > 1.0f) ? 1.0f : r * 2.0f,
			(g * 2.0f > 1.0f) ? 1.0f : g * 2.0f,
			(b * 2.0f > 1.0f) ? 1.0f : b * 2.0f,
			30.0f / 255.0f * f);
		blit.UVOffset = offsetUV;
		blit.Pad = XMFLOAT2(0, 0);
		ctx->UpdateSubresource(m_cbBlit, 0, nullptr, &blit, 0, 0);
		ctx->OMSetBlendState(m_blendAlpha, blendFactor, 0xffffffff);
		DrawFullscreen(ctx);

		/* Pass 2: additive aligned. */
		blit.Color = XMFLOAT4(r, g, b, a);
		blit.UVOffset = XMFLOAT2(0, 0);
		ctx->UpdateSubresource(m_cbBlit, 0, nullptr, &blit, 0, 0);
		ctx->OMSetBlendState(m_blendAdditive, blendFactor, 0xffffffff);
		DrawFullscreen(ctx);

		/* Pass 3: additive offset (BlurOn trails). */
		blit.UVOffset = offsetUV;
		ctx->UpdateSubresource(m_cbBlit, 0, nullptr, &blit, 0, 0);
		DrawFullscreen(ctx);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		ctx->PSSetShaderResources(0, 1, &nullSRV);
	}

	/* Save current frame (with overlay) for next frame's trails. */
	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	ctx->CopyResource(m_frontTex, backBuf);
	m_justInitialised = false;
}

void PostFX::Apply(DXRender* render)
{
	if (!m_vs || m_mode == MODE_OFF)
		return;

	ID3D11Texture2D* backBuf = render->GetBackBufferTexture();
	if (!backBuf)
		return;

	if (m_mode == MODE_COLOUR_FILTER) {
		if (!m_psColour || !m_backTex)
			return;
		ApplyColourFilter(render, backBuf);
	} else if (m_mode == MODE_MOTION_BLUR) {
		if (!m_psBlit || !m_frontTex)
			return;
		ApplyMotionBlur(render, backBuf);
	}

	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
