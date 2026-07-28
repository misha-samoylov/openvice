#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * Screen-space volumetric god rays from the directional sun.
 * Half-res radial scatter → additive upsample onto the back buffer.
 */
class GodRays
{
public:
	static constexpr float EXPOSURE = 0.145f;
	static constexpr float DECAY = 0.95f;
	static constexpr float DENSITY = 0.70f;
	static constexpr float WEIGHT = 0.266f;
	static constexpr float THRESHOLD = 0.68f;
	static constexpr float INTENSITY = 0.484f;
	/* Reversed-Z? No — standard D3D: far plane ≈ 1.0. Sky / distant depth. */
	static constexpr float DEPTH_CUTOFF = 0.992f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	/* sunDirToward = unit vector toward the sun (same as ShadowMap). */
	void Apply(DXRender* render, Camera* camera, FXMVECTOR sunDirToward);

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets();
	void DrawFullscreen(ID3D11DeviceContext* ctx);
	float ProjectSun(
		Camera* camera,
		FXMVECTOR sunDirToward,
		float& outU,
		float& outV,
		float& outOcclusion
	);

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_psRays;
	ID3D11PixelShader* m_psComposite;
	ID3D11Buffer* m_cb;
	ID3D11SamplerState* m_pointSampler;
	ID3D11SamplerState* m_linearSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthDisabled;
	ID3D11BlendState* m_blendOpaque;
	ID3D11BlendState* m_blendAdditive;

	/* Full-res scene color copy (sample while writing rays). */
	ID3D11Texture2D* m_colorTex;
	ID3D11ShaderResourceView* m_colorSRV;

	/* Half-res ray accumulation. */
	ID3D11Texture2D* m_raysTex;
	ID3D11RenderTargetView* m_raysRTV;
	ID3D11ShaderResourceView* m_raysSRV;

	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;
};
