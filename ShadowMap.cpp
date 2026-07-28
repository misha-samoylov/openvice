#include "ShadowMap.h"
#include <stdio.h>
#include <cmath>

HRESULT ShadowMap::Init(DXRender* render)
{
	m_texture = nullptr;
	m_dsv = nullptr;
	m_srv = nullptr;
	m_cmpSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthState = nullptr;
	m_lightViewProj = XMMatrixIdentity();

	/* Sun ~56° from zenith, azimuth toward -Z. */
	const float zenith = XMConvertToRadians(SUN_ZENITH_OFFSET_DEG);
	const float azimuth = 0.0f;
	m_sunDir = XMVector3Normalize(XMVectorSet(
		sinf(zenith) * sinf(azimuth),
		cosf(zenith),
		sinf(zenith) * cosf(azimuth),
		0.0f));

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;

	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = MAP_SIZE;
	texDesc.Height = MAP_SIZE;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = device->CreateTexture2D(&texDesc, nullptr, &m_texture);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateTexture2D failed\n");
		return hr;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	hr = device->CreateDepthStencilView(m_texture, &dsvDesc, &m_dsv);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateDepthStencilView failed\n");
		return hr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(m_texture, &srvDesc, &m_srv);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateShaderResourceView failed\n");
		return hr;
	}

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.BorderColor[0] = 1.0f;
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sampDesc, &m_cmpSampler);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateSamplerState failed\n");
		return hr;
	}

	D3D11_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(rsDesc));
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.DepthBias = 100;
	rsDesc.DepthBiasClamp = 0.0f;
	rsDesc.SlopeScaledDepthBias = 1.0f;
	rsDesc.DepthClipEnable = TRUE;
	hr = device->CreateRasterizerState(&rsDesc, &m_rasterizer);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateRasterizerState failed\n");
		return hr;
	}

	D3D11_DEPTH_STENCIL_DESC dsDesc;
	ZeroMemory(&dsDesc, sizeof(dsDesc));
	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	hr = device->CreateDepthStencilState(&dsDesc, &m_depthState);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateDepthStencilState failed\n");
		return hr;
	}

	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = (float)MAP_SIZE;
	m_viewport.Height = (float)MAP_SIZE;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	printf("[Info] ShadowMap ready (%ux%u), radius=%.0fm, sun %.0f deg from zenith\n",
		MAP_SIZE, MAP_SIZE, CASCADE_HALF_EXTENT, SUN_ZENITH_OFFSET_DEG);
	return S_OK;
}

void ShadowMap::Cleanup()
{
	if (m_depthState) { m_depthState->Release(); m_depthState = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_cmpSampler) { m_cmpSampler->Release(); m_cmpSampler = nullptr; }
	if (m_srv) { m_srv->Release(); m_srv = nullptr; }
	if (m_dsv) { m_dsv->Release(); m_dsv = nullptr; }
	if (m_texture) { m_texture->Release(); m_texture = nullptr; }
}

void ShadowMap::UpdateLight(float focusX, float focusY, float focusZ)
{
	/*
	 * Light travels opposite of sun direction (sun → ground).
	 * At 10° from zenith this is nearly -Y — NEVER use world-up as LookAt up,
	 * or the view basis collapses and shadows freeze / break when focus moves.
	 */
	XMVECTOR lightDir = XMVector3Normalize(XMVectorNegate(m_sunDir));

	XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR altUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	float align = fabsf(XMVectorGetX(XMVector3Dot(lightDir, worldUp)));
	XMVECTOR up = (align > 0.9f) ? altUp : worldUp;

	/* Stable right / true-up orthonormal frame around lightDir. */
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, lightDir));
	up = XMVector3Normalize(XMVector3Cross(lightDir, right));

	float extent = CASCADE_HALF_EXTENT * 2.0f;
	float texelWorld = extent / (float)MAP_SIZE;

	/*
	 * Snap focus to the light-space texel grid so the cascade does not shimmer
	 * as Tommy walks (and so large world coords stay stable).
	 */
	XMVECTOR focus = XMVectorSet(focusX, focusY, focusZ, 1.0f);
	float fx = XMVectorGetX(XMVector3Dot(focus, right));
	float fy = XMVectorGetX(XMVector3Dot(focus, up));
	fx = floorf(fx / texelWorld) * texelWorld;
	fy = floorf(fy / texelWorld) * texelWorld;
	float fz = XMVectorGetX(XMVector3Dot(focus, lightDir));

	XMVECTOR snapped =
		XMVectorScale(right, fx) +
		XMVectorScale(up, fy) +
		XMVectorScale(lightDir, fz);

	XMVECTOR eye = XMVectorSubtract(snapped, XMVectorScale(lightDir, CASCADE_DEPTH * 0.5f));
	XMMATRIX view = XMMatrixLookAtLH(eye, XMVectorAdd(eye, lightDir), up);
	XMMATRIX proj = XMMatrixOrthographicLH(extent, extent, 1.0f, CASCADE_DEPTH);
	m_lightViewProj = XMMatrixMultiply(view, proj);
}

void ShadowMap::Begin(DXRender* render)
{
	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	/* Unbind shadow SRV before writing depth. */
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(1, 1, &nullSRV);

	ctx->OMSetRenderTargets(0, nullptr, m_dsv);
	ctx->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
	ctx->RSSetViewports(1, &m_viewport);
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthState, 0);

	float blendFactor[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

void ShadowMap::End(DXRender* render)
{
	/* Restore main RTV/DSV and viewport via RenderStart's targets. */
	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
