#include "SSAO.h"

#include <stdio.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct SSAOCB
{
	XMFLOAT4X4 Proj;
	XMFLOAT2 InvFullSize;
	XMFLOAT2 InvHalfSize;
	float Radius;
	float Bias;
	float Intensity;
	float Power;
	float Proj33;
	float Proj43;
	float Proj11;
	float Proj22;
};

HRESULT SSAO::CreateHalfResTargets(DXRender* render)
{
	ReleaseHalfResTargets();

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = m_halfW;
	td.Height = m_halfH;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = device->CreateTexture2D(&td, nullptr, &m_aoTex);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRenderTargetView(m_aoTex, nullptr, &m_aoRTV);
	if (FAILED(hr))
		return hr;
	hr = device->CreateShaderResourceView(m_aoTex, nullptr, &m_aoSRV);
	if (FAILED(hr))
		return hr;

	hr = device->CreateTexture2D(&td, nullptr, &m_blurTex);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRenderTargetView(m_blurTex, nullptr, &m_blurRTV);
	if (FAILED(hr))
		return hr;
	hr = device->CreateShaderResourceView(m_blurTex, nullptr, &m_blurSRV);
	return hr;
}

void SSAO::ReleaseHalfResTargets()
{
	if (m_blurSRV) { m_blurSRV->Release(); m_blurSRV = nullptr; }
	if (m_blurRTV) { m_blurRTV->Release(); m_blurRTV = nullptr; }
	if (m_blurTex) { m_blurTex->Release(); m_blurTex = nullptr; }
	if (m_aoSRV) { m_aoSRV->Release(); m_aoSRV = nullptr; }
	if (m_aoRTV) { m_aoRTV->Release(); m_aoRTV = nullptr; }
	if (m_aoTex) { m_aoTex->Release(); m_aoTex = nullptr; }
}

HRESULT SSAO::Init(DXRender* render)
{
	m_vs = nullptr;
	m_psAO = nullptr;
	m_psBlur = nullptr;
	m_psComposite = nullptr;
	m_cb = nullptr;
	m_pointSampler = nullptr;
	m_linearSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthDisabled = nullptr;
	m_blendOpaque = nullptr;
	m_blendMultiply = nullptr;
	m_aoTex = nullptr;
	m_aoRTV = nullptr;
	m_aoSRV = nullptr;
	m_blurTex = nullptr;
	m_blurRTV = nullptr;
	m_blurSRV = nullptr;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;
	ID3DBlob* blob = nullptr;

	hr = D3DReadFileToBlob(L"ssao_vs.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: SSAO cannot read ssao_vs.cso\n");
		return hr;
	}
	hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_vs);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"ssao_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: SSAO cannot read ssao_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psAO);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"ssao_blur_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: SSAO cannot read ssao_blur_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psBlur);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"ssao_composite_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: SSAO cannot read ssao_composite_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psComposite);
	blob->Release();
	if (FAILED(hr))
		return hr;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.ByteWidth = sizeof(SSAOCB);
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

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = device->CreateSamplerState(&sd, &m_linearSampler);
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

	/* Src * Dest — darken by AO. */
	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blendMultiply);
	if (FAILED(hr))
		return hr;

	hr = CreateHalfResTargets(render);
	if (FAILED(hr)) {
		printf("Error: SSAO CreateHalfResTargets failed\n");
		return hr;
	}

	printf("[Info] SSAO ready (%ux%u half-res)\n", m_halfW, m_halfH);
	return S_OK;
}

void SSAO::Cleanup()
{
	ReleaseHalfResTargets();
	if (m_blendMultiply) { m_blendMultiply->Release(); m_blendMultiply = nullptr; }
	if (m_blendOpaque) { m_blendOpaque->Release(); m_blendOpaque = nullptr; }
	if (m_depthDisabled) { m_depthDisabled->Release(); m_depthDisabled = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_linearSampler) { m_linearSampler->Release(); m_linearSampler = nullptr; }
	if (m_pointSampler) { m_pointSampler->Release(); m_pointSampler = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_psComposite) { m_psComposite->Release(); m_psComposite = nullptr; }
	if (m_psBlur) { m_psBlur->Release(); m_psBlur = nullptr; }
	if (m_psAO) { m_psAO->Release(); m_psAO = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}

void SSAO::DrawFullscreen(ID3D11DeviceContext* ctx)
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

void SSAO::Apply(DXRender* render, Camera* camera)
{
	if (!m_vs || !render->GetDepthSRV())
		return;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();
	ID3D11ShaderResourceView* depthSRV = render->GetDepthSRV();

	/* Must not sample depth while it is bound as DSV. */
	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, nullptr);

	SSAOCB cb;
	XMMATRIX proj = camera->GetProjection();
	XMStoreFloat4x4(&cb.Proj, XMMatrixTranspose(proj));
	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, proj);
	cb.InvFullSize = XMFLOAT2(1.0f / (float)m_fullW, 1.0f / (float)m_fullH);
	cb.InvHalfSize = XMFLOAT2(1.0f / (float)m_halfW, 1.0f / (float)m_halfH);
	cb.Radius = RADIUS;
	cb.Bias = BIAS;
	cb.Intensity = INTENSITY;
	cb.Power = POWER;
	cb.Proj33 = p._33;
	cb.Proj43 = p._43;
	cb.Proj11 = p._11;
	cb.Proj22 = p._22;
	ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);

	ctx->VSSetConstantBuffers(0, 1, &m_cb);
	ctx->PSSetConstantBuffers(0, 1, &m_cb);
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthDisabled, 0);
	float blendFactor[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(m_blendOpaque, blendFactor, 0xffffffff);

	D3D11_VIEWPORT halfVP;
	halfVP.TopLeftX = 0.0f;
	halfVP.TopLeftY = 0.0f;
	halfVP.Width = (float)m_halfW;
	halfVP.Height = (float)m_halfH;
	halfVP.MinDepth = 0.0f;
	halfVP.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &halfVP);

	/* ---- AO pass ---- */
	ctx->OMSetRenderTargets(1, &m_aoRTV, nullptr);
	ctx->PSSetShader(m_psAO, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &depthSRV);
	ctx->PSSetSamplers(0, 1, &m_pointSampler);
	DrawFullscreen(ctx);

	/* ---- Blur pass ---- */
	ctx->OMSetRenderTargets(1, &m_blurRTV, nullptr);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	ctx->PSSetShader(m_psBlur, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_aoSRV);
	ctx->PSSetShaderResources(1, 1, &depthSRV);
	ctx->PSSetSamplers(0, 1, &m_pointSampler);
	DrawFullscreen(ctx);

	/* ---- Composite multiply onto back buffer ---- */
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	ctx->PSSetShaderResources(1, 1, &nullSRV);
	render->BindColorTargetOnly();
	ctx->OMSetBlendState(m_blendMultiply, blendFactor, 0xffffffff);
	ctx->PSSetShader(m_psComposite, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_blurSRV);
	ctx->PSSetSamplers(0, 1, &m_linearSampler);
	DrawFullscreen(ctx);

	/* Unbind AO / depth SRVs before next frame writes depth. */
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	ctx->OMSetBlendState(m_blendOpaque, blendFactor, 0xffffffff);
	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
