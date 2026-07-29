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
	m_srvIndex = UINT_MAX;
	m_cmpSamplerIndex = UINT_MAX;
	m_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	for (UINT i = 0; i < NUM_CASCADES; i++) {
		m_dsvIndex[i] = UINT_MAX;
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

	HRESULT hr = render->CreateTexture2D(
		MAP_SIZE, MAP_SIZE, DXGI_FORMAT_R24G8_TYPELESS,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&m_texture, NUM_CASCADES, 1);
	if (FAILED(hr)) {
		printf("Error: ShadowMap CreateTexture2D failed\n");
		return hr;
	}

	for (UINT i = 0; i < NUM_CASCADES; i++) {
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		UINT idx = render->AllocDsvIndex();
		if (idx == UINT_MAX) {
			printf("Error: ShadowMap AllocDsvIndex[%u] failed\n", i);
			return E_FAIL;
		}
		render->GetDevice()->CreateDepthStencilView(
			m_texture, &dsvDesc, render->GetDsvCpu(idx));
		m_dsvIndex[i] = idx;
	}

	m_srvIndex = render->CreateTexture2DArraySrv(
		m_texture, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, NUM_CASCADES);
	if (m_srvIndex == UINT_MAX) {
		printf("Error: ShadowMap CreateTexture2DArraySrv failed\n");
		return E_FAIL;
	}

	D3D12_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampDesc.BorderColor[0] = 1.0f;
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;
	sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
	m_cmpSamplerIndex = render->CreateSampler(sampDesc);
	if (m_cmpSamplerIndex == UINT_MAX) {
		printf("Error: ShadowMap CreateSampler failed\n");
		return E_FAIL;
	}

	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = (float)MAP_SIZE;
	m_viewport.Height = (float)MAP_SIZE;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_scissor.left = 0;
	m_scissor.top = 0;
	m_scissor.right = (LONG)MAP_SIZE;
	m_scissor.bottom = (LONG)MAP_SIZE;

	printf("[Info] ShadowMap CSM ready (%ux%u x %u), splits=%.0f/%.0f/%.0f/%.0f, sun %.0f deg\n",
		MAP_SIZE, MAP_SIZE, NUM_CASCADES,
		SPLIT_0, SPLIT_1, SPLIT_2, SPLIT_3, SUN_ZENITH_OFFSET_DEG);
	return S_OK;
}

void ShadowMap::Cleanup()
{
	if (m_texture) {
		m_texture->Release();
		m_texture = nullptr;
	}
	m_srvIndex = UINT_MAX;
	m_cmpSamplerIndex = UINT_MAX;
	for (UINT i = 0; i < NUM_CASCADES; i++)
		m_dsvIndex[i] = UINT_MAX;
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

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();

	if (m_state != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		render->Transition(m_texture, m_state, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = render->GetDsvCpu(m_dsvIndex[cascadeIndex]);
	cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	cmd->RSSetViewports(1, &m_viewport);
	cmd->RSSetScissorRects(1, &m_scissor);
}

void ShadowMap::End(DXRender* render)
{
	if (m_state == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		render->Transition(m_texture, m_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
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
