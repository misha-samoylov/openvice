#pragma once

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

	static constexpr float INTENSITY = 0.45f;
	static constexpr float DEFAULT_R = 40.0f;
	static constexpr float DEFAULT_G = 40.0f;
	static constexpr float DEFAULT_B = 40.0f;
	static constexpr float DEFAULT_BLUR_ALPHA = 47.25f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	void SetBlurColor(float r, float g, float b);
	void SetIntensity(float intensity);
	void SetBlurAlpha(float alpha);
	void SetMode(Mode mode);
	Mode CycleMode();
	Mode GetMode() const { return m_mode; }
	static const char* ModeName(Mode mode);

	void Apply(DXRender* render);

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets(DXRender* render = nullptr);
	void DrawFullscreen(DXRender* render, ID3D12PipelineState* pso, UINT srvIndex);
	void CopyBackBuffer(DXRender* render, ID3D12Resource* dest, D3D12_RESOURCE_STATES& destState);
	void ApplyColourFilter(DXRender* render);
	void ApplyMotionBlur(DXRender* render);

	ID3D12RootSignature* m_rootSig;
	ID3D12PipelineState* m_psoColour;
	ID3D12PipelineState* m_psoBlitOpaque;
	ID3D12PipelineState* m_psoBlitAlpha;
	ID3D12PipelineState* m_psoBlitAdditive;

	UINT m_pointSampler;

	ID3D12Resource* m_backTex;
	UINT m_backSrv;
	D3D12_RESOURCE_STATES m_backState;

	ID3D12Resource* m_frontTex;
	UINT m_frontSrv;
	D3D12_RESOURCE_STATES m_frontState;

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
