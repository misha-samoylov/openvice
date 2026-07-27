#include "PostFX.h"

#include <stdio.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct PostFXCB
{
	XMFLOAT4 BlurColor;
};

HRESULT PostFX::CreateSceneCopy(DXRender* render)
{
	ReleaseSceneCopy();

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
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &m_sceneTex);
	if (FAILED(hr))
		return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(m_sceneTex, &srvDesc, &m_sceneSRV);
	return hr;
}

void PostFX::ReleaseSceneCopy()
{
	if (m_sceneSRV) { m_sceneSRV->Release(); m_sceneSRV = nullptr; }
	if (m_sceneTex) { m_sceneTex->Release(); m_sceneTex = nullptr; }
}

HRESULT PostFX::Init(DXRender* render)
{
	m_vs = nullptr;
	m_ps = nullptr;
	m_cb = nullptr;
	m_pointSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthDisabled = nullptr;
	m_blendOpaque = nullptr;
	m_sceneTex = nullptr;
	m_sceneSRV = nullptr;
	m_width = m_height = 0;
	m_blurR = DEFAULT_R;
	m_blurG = DEFAULT_G;
	m_blurB = DEFAULT_B;
	m_intensity = INTENSITY;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;
	ID3DBlob* blob = nullptr;

	/* Reuse SSAO fullscreen triangle VS. */
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
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_ps);
	blob->Release();
	if (FAILED(hr))
		return hr;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.ByteWidth = sizeof(PostFXCB);
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&cbd, nullptr, &m_cb);
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

	hr = CreateSceneCopy(render);
	if (FAILED(hr)) {
		printf("Error: PostFX CreateSceneCopy failed\n");
		return hr;
	}

	printf("[Info] PostFX POSTFX_NORMAL ready (%ux%u)\n", m_width, m_height);
	return S_OK;
}

void PostFX::Cleanup()
{
	ReleaseSceneCopy();
	if (m_blendOpaque) { m_blendOpaque->Release(); m_blendOpaque = nullptr; }
	if (m_depthDisabled) { m_depthDisabled->Release(); m_depthDisabled = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_pointSampler) { m_pointSampler->Release(); m_pointSampler = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
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

void PostFX::Apply(DXRender* render)
{
	if (!m_vs || !m_ps || !m_sceneTex)
		return;

	ID3D11Texture2D* backBuf = render->GetBackBufferTexture();
	if (!backBuf)
		return;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	/* Unbind RTV before copying from the swap-chain back buffer. */
	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, nullptr);

	ctx->CopyResource(m_sceneTex, backBuf);

	PostFXCB cb;
	float f = m_intensity;
	cb.BlurColor = XMFLOAT4(
		m_blurR * f / 255.0f,
		m_blurG * f / 255.0f,
		m_blurB * f / 255.0f,
		30.0f / 255.0f);
	ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);

	render->BindColorTargetOnly();
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthDisabled, 0);
	float blendFactor[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(m_blendOpaque, blendFactor, 0xffffffff);

	ctx->PSSetShader(m_ps, nullptr, 0);
	ctx->PSSetConstantBuffers(0, 1, &m_cb);
	ctx->PSSetShaderResources(0, 1, &m_sceneSRV);
	ctx->PSSetSamplers(0, 1, &m_pointSampler);
	DrawFullscreen(ctx);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
