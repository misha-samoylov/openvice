#pragma once

#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * Screen-space ambient occlusion (depth-only).
 * Half-res AO → bilateral blur → multiply onto the back buffer.
 */
class SSAO
{
public:
	static constexpr float RADIUS = 1.15f;
	static constexpr float BIAS = 0.03f;
	static constexpr float INTENSITY = 1.35f;
	static constexpr float POWER = 3.6f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Call after the scene has written depth; multiplies AO onto color. */
	void Apply(DXRender* render, Camera* camera);

private:
	HRESULT CreateHalfResTargets(DXRender* render);
	void ReleaseHalfResTargets(DXRender* render = nullptr);
	void DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
		UINT srv0, UINT srv1, UINT samplerIndex);

	ID3D12RootSignature* m_rootSig;
	ID3D12PipelineState* m_psoAO;
	ID3D12PipelineState* m_psoBlur;
	ID3D12PipelineState* m_psoComposite;

	UINT m_pointSampler;
	UINT m_linearSampler;

	ID3D12Resource* m_aoTex;
	UINT m_aoRtv;
	UINT m_aoSrv;
	D3D12_RESOURCE_STATES m_aoState;

	ID3D12Resource* m_blurTex;
	UINT m_blurRtv;
	UINT m_blurSrv;
	D3D12_RESOURCE_STATES m_blurState;

	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;
};
