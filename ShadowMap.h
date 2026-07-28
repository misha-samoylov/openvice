#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"

using namespace DirectX;

/*
 * Directional sun shadow map (Y-up engine space).
 * Sun sits SUN_ZENITH_OFFSET_DEG off zenith; rays cast toward the ground.
 * Cascade follows the focus (Tommy / vehicle) each frame within RADIUS meters.
 */
class ShadowMap
{
public:
	static const UINT MAP_SIZE = 4096;
	static constexpr float SUN_ZENITH_OFFSET_DEG = -56.4f; /* ~20% lower elevation vs -48 */
	/* Half-extent of orthographic cascade (= radius around Tommy). */
	static constexpr float CASCADE_HALF_EXTENT = 500.0f;
	static constexpr float CASCADE_DEPTH = 1200.0f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Rebuild light view-proj around focus (player / vehicle / camera). */
	void UpdateLight(float focusX, float focusY, float focusZ);

	void Begin(DXRender* render);
	void End(DXRender* render);

	XMMATRIX GetLightViewProj() const { return m_lightViewProj; }
	XMVECTOR GetSunDirection() const { return m_sunDir; } /* unit vector toward sun */
	ID3D11ShaderResourceView* GetSRV() const { return m_srv; }
	ID3D11SamplerState* GetCmpSampler() const { return m_cmpSampler; }

private:
	ID3D11Texture2D* m_texture;
	ID3D11DepthStencilView* m_dsv;
	ID3D11ShaderResourceView* m_srv;
	ID3D11SamplerState* m_cmpSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthState;

	XMMATRIX m_lightViewProj;
	XMVECTOR m_sunDir;
	D3D11_VIEWPORT m_viewport;
};
