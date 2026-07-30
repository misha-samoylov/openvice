#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * Simple hybrid RT: quarter-res fullscreen RayQuery with 2 rays —
 *   1) sun visibility (hard shadow)
 *   2) one bounce (reflect) + sun check from hit
 * Mesh pixel shaders stay raster-only (avoids TDR from per-draw RayQuery).
 */
class RtBouncePass
{
public:
	HRESULT Init(DXRender* render);
	void Cleanup();

	void Apply(
		DXRender* render,
		Camera* camera,
		FXMVECTOR sunDirToward,
		D3D12_GPU_VIRTUAL_ADDRESS tlasVA);

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets(DXRender* render);
	void DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
		UINT colorSrv, UINT depthSrv, D3D12_GPU_VIRTUAL_ADDRESS tlasVA, UINT sampler);

	ID3D12RootSignature* m_rootSig = nullptr;
	ID3D12PipelineState* m_psoTrace = nullptr;
	ID3D12PipelineState* m_psoComposite = nullptr;

	UINT m_pointSampler = UINT_MAX;
	UINT m_linearSampler = UINT_MAX;

	ID3D12Resource* m_colorTex = nullptr;
	UINT m_colorSrv = UINT_MAX;
	D3D12_RESOURCE_STATES m_colorState = D3D12_RESOURCE_STATE_COMMON;

	ID3D12Resource* m_rtTex = nullptr;
	UINT m_rtRtv = UINT_MAX;
	UINT m_rtSrv = UINT_MAX;
	D3D12_RESOURCE_STATES m_rtState = D3D12_RESOURCE_STATE_COMMON;

	UINT m_fullW = 0;
	UINT m_fullH = 0;
	UINT m_rtW = 0;
	UINT m_rtH = 0;
};
