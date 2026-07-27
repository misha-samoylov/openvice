#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"

using namespace DirectX;

/*
 * re3 POSTFX_NORMAL:
 *  - Colour filter: colourfilterVC shader on current frame
 *  - Motion blur: classic CMBlur overlay from previous frame (+2px trails)
 */
class PostFX
{
public:
	enum Mode {
		MODE_OFF = 0,
		MODE_COLOUR_FILTER,
		MODE_MOTION_BLUR,
		MODE_COUNT
	};

	/* Colour-filter strength (shader path). */
	static constexpr float INTENSITY = 0.45f;
	/* Timecycle-style blur RGB (0–255). */
	static constexpr float DEFAULT_R = 40.0f;
	static constexpr float DEFAULT_G = 40.0f;
	static constexpr float DEFAULT_B = 40.0f;
	/* Additive pass alpha for motion blur (0–255). */
	static constexpr float DEFAULT_BLUR_ALPHA = 45.0f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	void SetBlurColor(float r, float g, float b);
	void SetIntensity(float intensity);
	void SetBlurAlpha(float alpha);
	void SetMode(Mode mode);
	Mode CycleMode();
	Mode GetMode() const { return m_mode; }
	static const char* ModeName(Mode mode);

	/* Call after the scene (and SSAO) has finished writing color. */
	void Apply(DXRender* render);

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets();
	void DrawFullscreen(ID3D11DeviceContext* ctx);
	void ApplyColourFilter(DXRender* render, ID3D11Texture2D* backBuf);
	void ApplyMotionBlur(DXRender* render, ID3D11Texture2D* backBuf);

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_psColour;
	ID3D11PixelShader* m_psBlit;
	ID3D11Buffer* m_cbColour;
	ID3D11Buffer* m_cbBlit;
	ID3D11SamplerState* m_pointSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthDisabled;
	ID3D11BlendState* m_blendOpaque;
	ID3D11BlendState* m_blendAlpha;
	ID3D11BlendState* m_blendAdditive;

	/* Current-frame copy for colour filter. */
	ID3D11Texture2D* m_backTex;
	ID3D11ShaderResourceView* m_backSRV;
	/* Previous-frame copy for motion blur. */
	ID3D11Texture2D* m_frontTex;
	ID3D11ShaderResourceView* m_frontSRV;

	UINT m_width;
	UINT m_height;

	Mode m_mode;
	bool m_justInitialised;

	float m_blurR;
	float m_blurG;
	float m_blurB;
	float m_blurAlpha;
	float m_intensity;
};
