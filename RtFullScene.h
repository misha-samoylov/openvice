#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

#include "DXRender.hpp"
#include "Camera.hpp"
#include "world/Scene.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
 * Full-screen primary RayQuery:
 * camera ray → mesh / sea plane → sun shadow + sky/sun disc.
 * Optional half-res path for dense map demos (ENABLE_RT_FULL_HALF_RES).
 */
class RtFullScene
{
public:
	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Bake shade triangles to match TLAS instance order. Call after TLAS Build. */
	bool BuildShadeData(DXRender* render, const Scene& scene, class RayTracedShadows* rt);

	void Apply(
		DXRender* render,
		Camera* camera,
		FXMVECTOR sunDirToward,
		D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
		float seaLevelY = 5.5f,
		float timeSec = 0.0f,
		UINT waterTexSrv = UINT_MAX,
		float waterUvU = 0.0f,
		float waterUvV = 0.0f);

	bool IsReady() const { return m_ready; }

private:
	HRESULT CreateHalfTargets(DXRender* render);
	void ReleaseHalfTargets(DXRender* render);
	void DrawTrace(
		DXRender* render,
		D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddr);
	void DrawUpsample(DXRender* render);

	ID3D12RootSignature* m_rootSig = nullptr;
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_upRootSig = nullptr;
	ID3D12PipelineState* m_upPso = nullptr;

	ComPtr<ID3D12Resource> m_triBuffer;
	ComPtr<ID3D12Resource> m_instBuffer;
	UINT m_triSrv = UINT_MAX;
	UINT m_instSrv = UINT_MAX;
	UINT m_triCount = 0;
	UINT m_instCount = 0;
	bool m_ready = false;

	ID3D12Resource* m_halfTex = nullptr;
	D3D12_RESOURCE_STATES m_halfState = D3D12_RESOURCE_STATE_COMMON;
	UINT m_halfRtv = UINT_MAX;
	UINT m_halfSrv = UINT_MAX;
	UINT m_halfW = 0;
	UINT m_halfH = 0;
	UINT m_fullW = 0;
	UINT m_fullH = 0;
};
