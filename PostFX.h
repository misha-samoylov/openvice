#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"

using namespace DirectX;

/*
 * re3 POSTFX_NORMAL colour filter (shader path, no motion blur).
 * Copies the back buffer, then applies colourfilterVC in a fullscreen pass.
 */
class PostFX
{
public:
	/* re3 default is 1.0 — lower = less colour-filter punch / brightness. */
	static constexpr float INTENSITY = 0.45f;
	/* Timecycle blur RGB (0–255). Lower = darker / less bloom. */
	static constexpr float DEFAULT_R = 40.0f;
	static constexpr float DEFAULT_G = 40.0f;
	static constexpr float DEFAULT_B = 40.0f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	void SetBlurColor(float r, float g, float b);
	void SetIntensity(float intensity);

	/* Call after the scene (and SSAO) has finished writing color. */
	void Apply(DXRender* render);

private:
	HRESULT CreateSceneCopy(DXRender* render);
	void ReleaseSceneCopy();
	void DrawFullscreen(ID3D11DeviceContext* ctx);

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11Buffer* m_cb;
	ID3D11SamplerState* m_pointSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthDisabled;
	ID3D11BlendState* m_blendOpaque;

	ID3D11Texture2D* m_sceneTex;
	ID3D11ShaderResourceView* m_sceneSRV;

	UINT m_width;
	UINT m_height;

	float m_blurR;
	float m_blurG;
	float m_blurB;
	float m_intensity;
};
