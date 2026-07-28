#pragma once

#include <d3d11.h>
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
	ID3D11ShaderResourceView* GetSRV() const { return m_srv; }
	ID3D11SamplerState* GetCmpSampler() const { return m_cmpSampler; }

private:
	static float SplitEnd(UINT cascadeIndex);
	void BuildCascadeVP(UINT cascadeIndex, XMVECTOR focus,
		XMVECTOR lightDir, XMVECTOR right, XMVECTOR up);

	ID3D11Texture2D* m_texture;
	ID3D11DepthStencilView* m_dsv[NUM_CASCADES];
	ID3D11ShaderResourceView* m_srv;
	ID3D11SamplerState* m_cmpSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthState;

	XMMATRIX m_lightViewProj[NUM_CASCADES];
	float m_halfExtent[NUM_CASCADES];
	XMVECTOR m_sunDir;
	D3D11_VIEWPORT m_viewport;
};
