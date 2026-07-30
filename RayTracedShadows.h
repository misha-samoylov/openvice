#pragma once

#include <vector>
#include <unordered_map>

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "DXRender.hpp"
#include "world/Scene.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
 * Static DXR TLAS for map geometry. Pixel-shader RayQuery traces toward the sun.
 * Built once after Scene::BuildFromAssets (no player/vehicle instances).
 */
class RayTracedShadows
{
public:
	RayTracedShadows();
	~RayTracedShadows();

	RayTracedShadows(const RayTracedShadows&) = delete;
	RayTracedShadows& operator=(const RayTracedShadows&) = delete;

	bool Init(DXRender* render);
	void Cleanup();

	/* Build BLAS/TLAS from opaque + cutout scene instances. Call after FlushUploads. */
	bool Build(DXRender* render, const Scene& scene);

	bool IsReady() const { return m_ready; }
	UINT GetSrvIndex() const { return m_tlasSrvIndex; }
	bool HasBlas(Mesh* mesh) const
	{
		return mesh && m_blas.find(mesh) != m_blas.end();
	}
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const
	{
		return m_tlas ? m_tlas->GetGPUVirtualAddress() : 0;
	}

private:
	struct BlasEntry
	{
		ComPtr<ID3D12Resource> as;
		D3D12_GPU_VIRTUAL_ADDRESS address = 0;
	};

	bool BuildEmptyTlas(DXRender* render);
	bool EnsureScratch(UINT64 bytes);
	bool BuildBlasForMesh(DXRender* render, ID3D12GraphicsCommandList4* cmd, Mesh* mesh, BlasEntry* out);
	bool BuildTlas(
		DXRender* render,
		ID3D12GraphicsCommandList4* cmd,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances);

	void AppendInstances(
		const std::vector<SceneInstance>& instances,
		bool cutoutOnly,
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& out);

	DXRender* m_render = nullptr;
	ID3D12Device5* m_device5 = nullptr;
	bool m_ready = false;

	std::unordered_map<Mesh*, BlasEntry> m_blas;
	ComPtr<ID3D12Resource> m_tlas;
	ComPtr<ID3D12Resource> m_scratch;
	ComPtr<ID3D12Resource> m_instanceBuffer;
	UINT m_tlasSrvIndex = UINT_MAX;
};
