#include "GodRays.h"

#include <stdio.h>
#include <cmath>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct GodRaysCB
{
	XMFLOAT2 LightPosUV;
	float Exposure;
	float Decay;
	float Density;
	float Weight;
	float Threshold;
	float Intensity;
	float DepthCutoff;
	float SunOcclusion;
	XMFLOAT2 Pad;
};

HRESULT GodRays::CreateTargets(DXRender* render)
{
	ReleaseTargets();

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	ID3D11Device* device = render->GetDevice();
	ID3D11Texture2D* backBuf = render->GetBackBufferTexture();
	if (!backBuf)
		return E_FAIL;

	D3D11_TEXTURE2D_DESC colorDesc;
	backBuf->GetDesc(&colorDesc);
	colorDesc.SampleDesc.Count = 1;
	colorDesc.SampleDesc.Quality = 0;
	colorDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	colorDesc.MiscFlags = 0;
	colorDesc.CPUAccessFlags = 0;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;

	HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, &m_colorTex);
	if (FAILED(hr))
		return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = colorDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(m_colorTex, &srvDesc, &m_colorSRV);
	if (FAILED(hr))
		return hr;

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = m_halfW;
	td.Height = m_halfH;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = device->CreateTexture2D(&td, nullptr, &m_raysTex);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRenderTargetView(m_raysTex, nullptr, &m_raysRTV);
	if (FAILED(hr))
		return hr;
	hr = device->CreateShaderResourceView(m_raysTex, nullptr, &m_raysSRV);
	return hr;
}

void GodRays::ReleaseTargets()
{
	if (m_raysSRV) { m_raysSRV->Release(); m_raysSRV = nullptr; }
	if (m_raysRTV) { m_raysRTV->Release(); m_raysRTV = nullptr; }
	if (m_raysTex) { m_raysTex->Release(); m_raysTex = nullptr; }
	if (m_colorSRV) { m_colorSRV->Release(); m_colorSRV = nullptr; }
	if (m_colorTex) { m_colorTex->Release(); m_colorTex = nullptr; }
}

HRESULT GodRays::Init(DXRender* render)
{
	m_vs = nullptr;
	m_psRays = nullptr;
	m_psComposite = nullptr;
	m_cb = nullptr;
	m_pointSampler = nullptr;
	m_linearSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthDisabled = nullptr;
	m_blendOpaque = nullptr;
	m_blendAdditive = nullptr;
	m_colorTex = nullptr;
	m_colorSRV = nullptr;
	m_raysTex = nullptr;
	m_raysRTV = nullptr;
	m_raysSRV = nullptr;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;
	ID3DBlob* blob = nullptr;

	hr = D3DReadFileToBlob(L"ssao_vs.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: GodRays cannot read ssao_vs.cso\n");
		return hr;
	}
	hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_vs);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"godrays_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: GodRays cannot read godrays_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psRays);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DReadFileToBlob(L"godrays_composite_ps.cso", &blob);
	if (FAILED(hr)) {
		printf("Error: GodRays cannot read godrays_composite_ps.cso\n");
		return hr;
	}
	hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_psComposite);
	blob->Release();
	if (FAILED(hr))
		return hr;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.ByteWidth = sizeof(GodRaysCB);
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

	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blendAdditive);
	if (FAILED(hr))
		return hr;

	hr = CreateTargets(render);
	if (FAILED(hr)) {
		printf("Error: GodRays CreateTargets failed\n");
		return hr;
	}

	printf("[Info] GodRays ready (%ux%u half-res) — F10 toggles\n", m_halfW, m_halfH);
	return S_OK;
}

void GodRays::Cleanup()
{
	ReleaseTargets();
	if (m_blendAdditive) { m_blendAdditive->Release(); m_blendAdditive = nullptr; }
	if (m_blendOpaque) { m_blendOpaque->Release(); m_blendOpaque = nullptr; }
	if (m_depthDisabled) { m_depthDisabled->Release(); m_depthDisabled = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_linearSampler) { m_linearSampler->Release(); m_linearSampler = nullptr; }
	if (m_pointSampler) { m_pointSampler->Release(); m_pointSampler = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_psComposite) { m_psComposite->Release(); m_psComposite = nullptr; }
	if (m_psRays) { m_psRays->Release(); m_psRays = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}

float GodRays::ProjectSun(
	Camera* camera,
	FXMVECTOR sunDirToward,
	float& outU,
	float& outV,
	float& outOcclusion)
{
	outU = 0.5f;
	outV = 0.5f;
	outOcclusion = 0.0f;

	/* Directional light → screen UV via view-space direction (stable for any pitch). */
	XMMATRIX view = camera->GetView();
	XMVECTOR sunView = XMVector3TransformNormal(sunDirToward, view);
	float vx = XMVectorGetX(sunView);
	float vy = XMVectorGetY(sunView);
	float vz = XMVectorGetZ(sunView);

	/* Only cull when the sun is behind the camera. */
	if (vz <= 1e-4f)
		return 0.0f;

	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, camera->GetProjection());
	float ndcX = (vx / vz) * p._11;
	float ndcY = (vy / vz) * p._22;

	outU = ndcX * 0.5f + 0.5f;
	outV = -ndcY * 0.5f + 0.5f;

	/*
	 * Soft fade only when far outside the frame. Keep a strong floor so looking
	 * up (sun near / past the edge) still produces rays across the sky.
	 */
	float dx = fabsf(outU - 0.5f) * 2.0f;
	float dy = fabsf(outV - 0.5f) * 2.0f;
	float outside = fmaxf(0.0f, fmaxf(dx, dy) - 1.15f);
	float edge = 1.0f - outside * 0.35f;
	if (edge < 0.35f)
		edge = 0.35f;
	if (edge > 1.0f)
		edge = 1.0f;

	outOcclusion = edge;
	return edge;
}

void GodRays::DrawFullscreen(ID3D11DeviceContext* ctx)
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

void GodRays::Apply(DXRender* render, Camera* camera, FXMVECTOR sunDirToward)
{
	if (!m_vs || !m_psRays || !m_colorTex)
		return;

	ID3D11ShaderResourceView* depthSRV = render->GetDepthSRV();
	ID3D11Texture2D* backBuf = render->GetBackBufferTexture();
	if (!depthSRV || !backBuf)
		return;

	float sunU, sunV, sunOcc;
	if (ProjectSun(camera, sunDirToward, sunU, sunV, sunOcc) <= 0.001f)
		return;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	/* Unbind RT so we can copy / sample. */
	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	ctx->CopyResource(m_colorTex, backBuf);

	GodRaysCB cb;
	cb.LightPosUV = XMFLOAT2(sunU, sunV);
	cb.Exposure = EXPOSURE;
	cb.Decay = DECAY;
	cb.Density = DENSITY;
	cb.Weight = WEIGHT;
	cb.Threshold = THRESHOLD;
	cb.Intensity = INTENSITY;
	cb.DepthCutoff = DEPTH_CUTOFF;
	cb.SunOcclusion = sunOcc;
	cb.Pad = XMFLOAT2(0.0f, 0.0f);
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

	float clearRays[4] = { 0, 0, 0, 0 };
	ctx->ClearRenderTargetView(m_raysRTV, clearRays);
	ctx->OMSetRenderTargets(1, &m_raysRTV, nullptr);
	ctx->PSSetShader(m_psRays, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_colorSRV);
	ctx->PSSetShaderResources(1, 1, &depthSRV);
	ctx->PSSetSamplers(0, 1, &m_linearSampler);
	ctx->PSSetSamplers(1, 1, &m_pointSampler);
	DrawFullscreen(ctx);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	ctx->PSSetShaderResources(1, 1, &nullSRV);

	/* Additive upsample onto resolved back buffer. */
	render->BindBackBufferOnly();
	ctx->OMSetBlendState(m_blendAdditive, blendFactor, 0xffffffff);
	ctx->PSSetShader(m_psComposite, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_raysSRV);
	ctx->PSSetSamplers(0, 1, &m_linearSampler);
	DrawFullscreen(ctx);

	ctx->PSSetShaderResources(0, 1, &nullSRV);
	ctx->OMSetBlendState(m_blendOpaque, blendFactor, 0xffffffff);
	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
