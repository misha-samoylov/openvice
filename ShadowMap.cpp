#include "ShadowMap.h"
#include <stdio.h>
#include <cmath>

float ShadowMap::SplitEnd(UINT cascadeIndex)
{
	switch (cascadeIndex) {
	case 0: return SPLIT_0;
	case 1: return SPLIT_1;
	case 2: return SPLIT_2;
	default: return SPLIT_3;
	}
}

HRESULT ShadowMap::Init(DXRender* render)
{
	m_texture = nullptr;
	m_srv = nullptr;
	m_cmpSampler = nullptr;
	m_rasterizer = nullptr;
	m_depthState = nullptr;
	for (UINT i = 0; i < NUM_CASCADES; i++) {
		m_dsv[i] = nullptr;
		m_lightViewProj[i] = XMMatrixIdentity();
		m_halfExtent[i] = SplitEnd(i) * EXTENT_SCALE;
	}

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
	texDesc.ArraySize = NUM_CASCADES;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = device->CreateTexture2D(&texDesc, nullptr, &m_texture);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateTexture2D failed\n");
		return hr;
	}

	for (UINT i = 0; i < NUM_CASCADES; i++) {
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		ZeroMemory(&dsvDesc, sizeof(dsvDesc));
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		hr = device->CreateDepthStencilView(m_texture, &dsvDesc, &m_dsv[i]);
		if (FAILED(hr)) {
			printf("Error: ShadowMap CreateDepthStencilView[%u] failed\n", i);
			return hr;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = NUM_CASCADES;
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
	/*
	 * Near cascades have fine texels so a modest clamp (~22 cm) is enough.
	 * Receiver bias in the PS scales up for coarser far cascades.
	 */
	rsDesc.DepthBias = 16;
	rsDesc.DepthBiasClamp = 0.00018f;
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

	printf("[Info] ShadowMap CSM ready (%ux%u x %u), splits=%.0f/%.0f/%.0f/%.0f, sun %.0f deg\n",
		MAP_SIZE, MAP_SIZE, NUM_CASCADES,
		SPLIT_0, SPLIT_1, SPLIT_2, SPLIT_3, SUN_ZENITH_OFFSET_DEG);
	return S_OK;
}

void ShadowMap::Cleanup()
{
	if (m_depthState) { m_depthState->Release(); m_depthState = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_cmpSampler) { m_cmpSampler->Release(); m_cmpSampler = nullptr; }
	if (m_srv) { m_srv->Release(); m_srv = nullptr; }
	for (UINT i = 0; i < NUM_CASCADES; i++) {
		if (m_dsv[i]) { m_dsv[i]->Release(); m_dsv[i] = nullptr; }
	}
	if (m_texture) { m_texture->Release(); m_texture = nullptr; }
}

void ShadowMap::BuildCascadeVP(UINT cascadeIndex, XMVECTOR focus,
	XMVECTOR lightDir, XMVECTOR right, XMVECTOR up)
{
	float halfExtent = SplitEnd(cascadeIndex) * EXTENT_SCALE;
	m_halfExtent[cascadeIndex] = halfExtent;
	float extent = halfExtent * 2.0f;
	float texelWorld = extent / (float)MAP_SIZE;

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
	m_lightViewProj[cascadeIndex] = XMMatrixMultiply(view, proj);
}

void ShadowMap::UpdateCascades(float focusX, float focusY, float focusZ)
{
	XMVECTOR lightDir = XMVector3Normalize(XMVectorNegate(m_sunDir));

	XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR altUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	float align = fabsf(XMVectorGetX(XMVector3Dot(lightDir, worldUp)));
	XMVECTOR up = (align > 0.9f) ? altUp : worldUp;

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, lightDir));
	up = XMVector3Normalize(XMVector3Cross(lightDir, right));

	XMVECTOR focus = XMVectorSet(focusX, focusY, focusZ, 1.0f);
	for (UINT i = 0; i < NUM_CASCADES; i++)
		BuildCascadeVP(i, focus, lightDir, right, up);
}

void ShadowMap::Begin(DXRender* render, UINT cascadeIndex)
{
	if (cascadeIndex >= NUM_CASCADES)
		cascadeIndex = NUM_CASCADES - 1;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(1, 1, &nullSRV);

	ctx->OMSetRenderTargets(0, nullptr, m_dsv[cascadeIndex]);
	ctx->ClearDepthStencilView(m_dsv[cascadeIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
	ctx->RSSetViewports(1, &m_viewport);
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthState, 0);

	float blendFactor[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

void ShadowMap::End(DXRender* render)
{
	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}

XMMATRIX ShadowMap::GetLightViewProj(UINT cascadeIndex) const
{
	if (cascadeIndex >= NUM_CASCADES)
		cascadeIndex = NUM_CASCADES - 1;
	return m_lightViewProj[cascadeIndex];
}

float ShadowMap::GetCascadeHalfExtent(UINT cascadeIndex) const
{
	if (cascadeIndex >= NUM_CASCADES)
		cascadeIndex = NUM_CASCADES - 1;
	return m_halfExtent[cascadeIndex];
}

XMFLOAT4 ShadowMap::GetSplitDistances() const
{
	return XMFLOAT4(SPLIT_0, SPLIT_1, SPLIT_2, SPLIT_3);
}
