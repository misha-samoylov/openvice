#pragma once

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
	static constexpr float DEPTH_CUTOFF = 0.992f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	void Apply(DXRender* render, Camera* camera, FXMVECTOR sunDirToward);

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets(DXRender* render = nullptr);
	void DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
		UINT srv0, UINT srv1, UINT samplerIndex);
	float ProjectSun(
		Camera* camera,
		FXMVECTOR sunDirToward,
		float& outU,
		float& outV,
		float& outOcclusion
	);

	ID3D12RootSignature* m_rootSig;
	ID3D12PipelineState* m_psoRays;
	ID3D12PipelineState* m_psoComposite;

	UINT m_pointSampler;
	UINT m_linearSampler;

	ID3D12Resource* m_colorTex;
	UINT m_colorSrv;
	D3D12_RESOURCE_STATES m_colorState;

	ID3D12Resource* m_raysTex;
	UINT m_raysRtv;
	UINT m_raysSrv;
	D3D12_RESOURCE_STATES m_raysState;

	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;
};
