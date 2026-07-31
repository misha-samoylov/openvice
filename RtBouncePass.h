#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "DXRender.hpp"
#include "Camera.hpp"
#include "world/Scene.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
 * Master-look RT sun shadows + RTAO (DX12):
 *   soft RayQuery sun shadows with alpha punch-through,
 *   plus short-range ray-traced ambient occlusion.
 *   color *= lerp(0.625, 1.0, lit) * ao.
 */
class RtBouncePass
{
public:
	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Bake tris/UV/tex to match TLAS InstanceID order. Call after TLAS Build. */
	bool BuildShadeData(DXRender* render, const Scene& scene, class RayTracedShadows* rt);

	void Apply(
		DXRender* render,
		Camera* camera,
		FXMVECTOR sunDirToward,
		D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
		bool enableShadows = true,
		bool enableRtao = true);

	bool HasShadeData() const { return m_shadeReady; }

private:
	HRESULT CreateTargets(DXRender* render);
	void ReleaseTargets(DXRender* render);
	void ReleaseShade();
	void DrawTrace(DXRender* render, D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddr, UINT depthSrv);
	void DrawComposite(DXRender* render, D3D12_GPU_VIRTUAL_ADDRESS tlasVA);

	ID3D12RootSignature* m_rootSig = nullptr;
	ID3D12PipelineState* m_psoTrace = nullptr;
	ID3D12RootSignature* m_compRootSig = nullptr;
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

	ComPtr<ID3D12Resource> m_triBuffer;
	ComPtr<ID3D12Resource> m_instBuffer;
	UINT m_triSrv = UINT_MAX;
	UINT m_instSrv = UINT_MAX;
	bool m_shadeReady = false;

	UINT m_fullW = 0;
	UINT m_fullH = 0;
	UINT m_rtW = 0;
	UINT m_rtH = 0;
};
