#pragma once

#include <DirectXMath.h>

#include "DXRender.hpp"

using namespace DirectX;

/*
 * Cascaded directional sun shadows (Y-up engine space).
 * Sun sits SUN_ZENITH_OFFSET_DEG off zenith; rays cast toward the ground.
 * Four ortho cascades follow the focus; split ends are view-space distances.
 */
class ShadowMap
{
public:
	static const UINT NUM_CASCADES = 4;
	static const UINT MAP_SIZE = 2048;
	static constexpr float SUN_ZENITH_OFFSET_DEG = -56.4f;
	/* View-space cascade end distances (meters from camera). */
	static constexpr float SPLIT_0 = 25.0f;
	static constexpr float SPLIT_1 = 80.0f;
	static constexpr float SPLIT_2 = 200.0f;
	static constexpr float SPLIT_3 = 500.0f;
	static constexpr float CASCADE_DEPTH = 1200.0f;
	/* Ortho half-extent = splitEnd * EXTENT_SCALE (padding around focus). */
	static constexpr float EXTENT_SCALE = 1.1f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Rebuild all cascade light view-projs around focus. */
	void UpdateCascades(float focusX, float focusY, float focusZ);

	void Begin(DXRender* render, UINT cascadeIndex);
	void End(DXRender* render);

	XMMATRIX GetLightViewProj(UINT cascadeIndex) const;
	float GetCascadeHalfExtent(UINT cascadeIndex) const;
	XMFLOAT4 GetSplitDistances() const;
	float GetMaxRange() const { return SPLIT_3; }

	XMVECTOR GetSunDirection() const { return m_sunDir; }
	UINT GetSrvIndex() const { return m_srvIndex; }
	UINT GetCmpSamplerIndex() const { return m_cmpSamplerIndex; }

private:
	static float SplitEnd(UINT cascadeIndex);
	void BuildCascadeVP(UINT cascadeIndex, XMVECTOR focus,
		XMVECTOR lightDir, XMVECTOR right, XMVECTOR up);

	ID3D12Resource* m_texture;
	UINT m_dsvIndex[NUM_CASCADES];
	UINT m_srvIndex;
	UINT m_cmpSamplerIndex;
	D3D12_RESOURCE_STATES m_state;

	XMMATRIX m_lightViewProj[NUM_CASCADES];
	float m_halfExtent[NUM_CASCADES];
	XMVECTOR m_sunDir;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissor;
};
