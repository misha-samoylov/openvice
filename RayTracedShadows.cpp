#include "RayTracedShadows.h"
#include "core/GameConfig.h"
#include "Model.h"

#include <stdio.h>
#include <string.h>

RayTracedShadows::RayTracedShadows() = default;

RayTracedShadows::~RayTracedShadows()
{
	Cleanup();
}

bool RayTracedShadows::Init(DXRender* render)
{
	Cleanup();
	if (!render || !render->SupportsRaytracing()) {
		printf("[Warn] RayTracedShadows: GPU has no DXR — sun RT shadows disabled\n");
		return false;
	}
	m_render = render;
	m_device5 = render->GetDevice5();
	if (!m_device5)
		return false;

	/* Empty TLAS so t2 is always a valid AS SRV (never bind a Texture2D there). */
	if (!BuildEmptyTlas(render)) {
		printf("[Error] RayTracedShadows: empty TLAS failed\n");
		Cleanup();
		return false;
	}
	return true;
}

void RayTracedShadows::Cleanup()
{
	m_blas.clear();
	m_tlas.Reset();
	m_scratch.Reset();
	m_instanceBuffer.Reset();
	m_tlasSrvIndex = UINT_MAX;
	m_ready = false;
	m_device5 = nullptr;
	m_render = nullptr;
}

static void StoreInstanceTransform(const XMMATRIX& world, D3D12_RAYTRACING_INSTANCE_DESC* desc)
{
	/* DXR float3x4 is applied as mul(M, float4(p,1)) (column vector).
	 * DirectXMath uses row vectors (p' = p * W), so pack columns of W:
	 *   row0 = (_11,_21,_31,_41), etc. — NOT the matrix memory rows. */
	XMFLOAT4X4 m;
	XMStoreFloat4x4(&m, world);
	desc->Transform[0][0] = m._11; desc->Transform[0][1] = m._21;
	desc->Transform[0][2] = m._31; desc->Transform[0][3] = m._41;
	desc->Transform[1][0] = m._12; desc->Transform[1][1] = m._22;
	desc->Transform[1][2] = m._32; desc->Transform[1][3] = m._42;
	desc->Transform[2][0] = m._13; desc->Transform[2][1] = m._23;
	desc->Transform[2][2] = m._33; desc->Transform[2][3] = m._43;
}

static XMMATRIX InstanceWorld(const SceneInstance& inst)
{
	return XMMatrixRotationQuaternion(XMVectorSet(
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3])) *
		XMMatrixScaling(inst.scale[0], inst.scale[1], inst.scale[2]) *
		XMMatrixTranslation(inst.x, inst.y, inst.z);
}

static bool FillBlasGeom(Mesh* mesh, D3D12_RAYTRACING_GEOMETRY_DESC* geom)
{
	if (!mesh || !geom)
		return false;
	ID3D12Resource* vb = mesh->GetVertexBuffer();
	ID3D12Resource* ib = mesh->GetIndexBuffer();
	UINT indexCount = mesh->GetIndexCount();
	int vertexCount = mesh->GetVertexCount();
	if (!vb || !ib || indexCount < 3 || (indexCount % 3) != 0 || vertexCount < 3)
		return false;
	if (mesh->GetPrimitiveTopology() != D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		return false;

	*geom = {};
	geom->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geom->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geom->Triangles.Transform3x4 = 0;
	geom->Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geom->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geom->Triangles.IndexCount = indexCount;
	geom->Triangles.VertexCount = (UINT)vertexCount;
	geom->Triangles.IndexBuffer = ib->GetGPUVirtualAddress();
	geom->Triangles.VertexBuffer.StartAddress = vb->GetGPUVirtualAddress();
	geom->Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 5;
	return true;
}

bool RayTracedShadows::EnsureScratch(UINT64 bytes)
{
	if (bytes == 0)
		return true;
	if (m_scratch && m_scratch->GetDesc().Width >= bytes)
		return true;

	/* Never destroy an in-flight scratch — only allocate before recording builds. */
	m_scratch.Reset();
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = bytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	return SUCCEEDED(m_device5->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr, IID_PPV_ARGS(&m_scratch)));
}

bool RayTracedShadows::BuildBlasForMesh(
	DXRender* render, ID3D12GraphicsCommandList4* cmd, Mesh* mesh, BlasEntry* out)
{
	(void)render;
	if (!mesh || !out)
		return false;

	D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
	if (!FillBlasGeom(mesh, &geom))
		return false;

	ID3D12Resource* vb = mesh->GetVertexBuffer();
	ID3D12Resource* ib = mesh->GetIndexBuffer();

	/* BLAS inputs must be NON_PIXEL_SHADER_RESOURCE — wrong state can TDR on TraceRay. */
	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = vb;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[1] = barriers[0];
	barriers[1].Transition.pResource = ib;
	cmd->ResourceBarrier(2, barriers);

	auto restoreVbIb = [&]() {
		D3D12_RESOURCE_BARRIER back[2] = { barriers[0], barriers[1] };
		back[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		back[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		back[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		back[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		cmd->ResourceBarrier(2, back);
	};

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = 1;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geom;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
	if (prebuild.ResultDataMaxSizeInBytes == 0) {
		restoreVbIb();
		return false;
	}
	if (prebuild.ScratchDataSizeInBytes > 0 &&
		(!m_scratch || m_scratch->GetDesc().Width < prebuild.ScratchDataSizeInBytes)) {
		restoreVbIb();
		printf("[Error] RayTracedShadows: scratch too small for BLAS\n");
		return false;
	}

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC asDesc = {};
	asDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	asDesc.Width = prebuild.ResultDataMaxSizeInBytes;
	asDesc.Height = 1;
	asDesc.DepthOrArraySize = 1;
	asDesc.MipLevels = 1;
	asDesc.SampleDesc.Count = 1;
	asDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	asDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ComPtr<ID3D12Resource> as;
	if (FAILED(m_device5->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &asDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr, IID_PPV_ARGS(&as)))) {
		restoreVbIb();
		return false;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
	build.Inputs = inputs;
	build.ScratchAccelerationStructureData = m_scratch ? m_scratch->GetGPUVirtualAddress() : 0;
	build.DestAccelerationStructureData = as->GetGPUVirtualAddress();
	cmd->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

	D3D12_RESOURCE_BARRIER uav = {};
	uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav.UAV.pResource = as.Get();
	cmd->ResourceBarrier(1, &uav);

	restoreVbIb();

	out->as = as;
	out->address = as->GetGPUVirtualAddress();
	return true;
}

bool RayTracedShadows::BuildEmptyTlas(DXRender* render)
{
	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList4> buildList;
	if (FAILED(m_device5->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))))
		return false;
	{
		ComPtr<ID3D12GraphicsCommandList> list;
		if (FAILED(m_device5->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list))))
			return false;
		if (FAILED(list.As(&buildList)))
			return false;
	}

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> none;
	if (!BuildTlas(render, buildList.Get(), none))
		return false;

	buildList->Close();
	ID3D12CommandList* lists[] = { buildList.Get() };
	render->GetQueue()->ExecuteCommandLists(1, lists);
	render->WaitForGpu();
	return m_tlasSrvIndex != UINT_MAX;
}

bool RayTracedShadows::BuildTlas(
	DXRender* render,
	ID3D12GraphicsCommandList4* cmd,
	const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances)
{
	(void)render;
	const UINT num = (UINT)instances.size();

	D3D12_GPU_VIRTUAL_ADDRESS instanceAddr = 0;
	if (num > 0) {
		const UINT64 bytes = (UINT64)num * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		D3D12_HEAP_PROPERTIES uploadHeap = {};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC bufDesc = {};
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Width = bytes;
		bufDesc.Height = 1;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.MipLevels = 1;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		m_instanceBuffer.Reset();
		if (FAILED(m_device5->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_instanceBuffer))))
			return false;

		void* mapped = nullptr;
		m_instanceBuffer->Map(0, nullptr, &mapped);
		memcpy(mapped, instances.data(), (size_t)bytes);
		m_instanceBuffer->Unmap(0, nullptr);
		instanceAddr = m_instanceBuffer->GetGPUVirtualAddress();
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = num;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = instanceAddr;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
	if (prebuild.ResultDataMaxSizeInBytes == 0)
		return false;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC asDesc = {};
	asDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	asDesc.Width = prebuild.ResultDataMaxSizeInBytes;
	asDesc.Height = 1;
	asDesc.DepthOrArraySize = 1;
	asDesc.MipLevels = 1;
	asDesc.SampleDesc.Count = 1;
	asDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	asDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_tlas.Reset();
	if (FAILED(m_device5->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &asDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr, IID_PPV_ARGS(&m_tlas))))
		return false;

	UINT64 scratchSize = prebuild.ScratchDataSizeInBytes;
	if (scratchSize > 0 && !EnsureScratch(scratchSize))
		return false;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
	build.Inputs = inputs;
	build.ScratchAccelerationStructureData = m_scratch ? m_scratch->GetGPUVirtualAddress() : 0;
	build.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
	cmd->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

	D3D12_RESOURCE_BARRIER uav = {};
	uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav.UAV.pResource = m_tlas.Get();
	cmd->ResourceBarrier(1, &uav);

	m_tlasSrvIndex = render->CreateAccelerationStructureSrv(m_tlas.Get());
	return m_tlasSrvIndex != UINT_MAX;
}

void RayTracedShadows::AppendInstances(
	const std::vector<SceneInstance>& instances,
	bool cutoutOnly,
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& out)
{
	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		Model* model = inst.model;
		if (!model)
			continue;

		XMMATRIX world = InstanceWorld(inst);
		std::vector<Mesh*>& meshes = model->GetMeshes();
		for (size_t m = 0; m < meshes.size(); m++) {
			Mesh* mesh = meshes[m];
			if (!mesh)
				continue;
#if !ENABLE_SINGLE_OBJECT_RT_DEMO && !ENABLE_RT_FULL_SCENE
			if (cutoutOnly) {
				if (!mesh->GetAlpha() || !mesh->IsAlphaCutout())
					continue;
			} else {
				if (mesh->GetAlpha())
					continue;
			}
#else
			(void)cutoutOnly;
#endif

			std::unordered_map<Mesh*, BlasEntry>::iterator it = m_blas.find(mesh);
			if (it == m_blas.end())
				continue;

			D3D12_RAYTRACING_INSTANCE_DESC desc = {};
			StoreInstanceTransform(world, &desc);
			desc.InstanceID = (UINT)out.size() & 0xFFFFF;
			desc.InstanceMask = 0xFF;
			desc.InstanceContributionToHitGroupIndex = 0;
			desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			desc.AccelerationStructure = it->second.address;
			out.push_back(desc);
		}
	}
}

bool RayTracedShadows::Build(DXRender* render, const Scene& scene)
{
	m_ready = false;
	m_blas.clear();
	m_tlas.Reset();
	m_instanceBuffer.Reset();
	m_tlasSrvIndex = UINT_MAX;

	if (!m_device5) {
		if (!Init(render))
			return false;
	} else {
		m_render = render;
	}

	/* Collect unique meshes that cast hard shadows. */
	std::vector<Mesh*> unique;
	auto collect = [&](const std::vector<SceneInstance>& list, bool cutoutOnly) {
		for (size_t i = 0; i < list.size(); i++) {
			Model* model = list[i].model;
			if (!model)
				continue;
			std::vector<Mesh*>& meshes = model->GetMeshes();
			for (size_t m = 0; m < meshes.size(); m++) {
				Mesh* mesh = meshes[m];
				if (!mesh)
					continue;
#if ENABLE_RT_FULL_SCENE || ENABLE_SINGLE_OBJECT_RT_DEMO
				(void)cutoutOnly;
#else
				if (cutoutOnly) {
					if (!mesh->GetAlpha() || !mesh->IsAlphaCutout())
						continue;
				} else if (mesh->GetAlpha()) {
					continue;
				}
#endif
				if (m_blas.find(mesh) != m_blas.end())
					continue;
				m_blas[mesh] = BlasEntry();
				unique.push_back(mesh);
			}
		}
	};
	collect(scene.Opaque(), false);
#if ENABLE_RT_FULL_SCENE || ENABLE_SINGLE_OBJECT_RT_DEMO
	collect(scene.Alpha(), false);
#else
	/* Opaque casters only — cutout foliage in TLAS + soft overdraw was hanging the GPU. */
#endif

	/* Pre-size scratch BEFORE recording — growing/destroying scratch mid-list TDRs. */
	UINT64 maxBlasScratch = 0;
	for (size_t i = 0; i < unique.size(); i++) {
		D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
		if (!FillBlasGeom(unique[i], &geom))
			continue;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = 1;
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.pGeometryDescs = &geom;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
		m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
		if (prebuild.ScratchDataSizeInBytes > maxBlasScratch)
			maxBlasScratch = prebuild.ScratchDataSizeInBytes;
	}
	if (!EnsureScratch(maxBlasScratch)) {
		printf("[Error] RayTracedShadows: BLAS scratch alloc failed\n");
		return false;
	}

	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList4> buildList;
	if (FAILED(m_device5->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))))
		return false;
	{
		ComPtr<ID3D12GraphicsCommandList> list;
		if (FAILED(m_device5->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list))))
			return false;
		if (FAILED(list.As(&buildList)))
			return false;
	}

	int builtBlas = 0;
	for (size_t i = 0; i < unique.size(); i++) {
		Mesh* mesh = unique[i];
		BlasEntry entry;
		if (!BuildBlasForMesh(render, buildList.Get(), mesh, &entry)) {
			m_blas.erase(mesh);
			continue;
		}
		m_blas[mesh] = std::move(entry);
		builtBlas++;

		if (m_scratch) {
			D3D12_RESOURCE_BARRIER uav = {};
			uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uav.UAV.pResource = m_scratch.Get();
			buildList->ResourceBarrier(1, &uav);
		}
	}

	buildList->Close();
	{
		ID3D12CommandList* lists[] = { buildList.Get() };
		render->GetQueue()->ExecuteCommandLists(1, lists);
		render->WaitForGpu();
	}
	if (FAILED(m_device5->GetDeviceRemovedReason())) {
		printf("[Error] RayTracedShadows: device removed after BLAS (0x%08X)\n",
			(unsigned)m_device5->GetDeviceRemovedReason());
		return false;
	}

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
	instances.reserve(scene.Opaque().size() + scene.Alpha().size());
	AppendInstances(scene.Opaque(), false, instances);
#if ENABLE_RT_FULL_SCENE || ENABLE_SINGLE_OBJECT_RT_DEMO
	AppendInstances(scene.Alpha(), false, instances);
#endif

	/* Second submit for TLAS so scratch can grow safely after BLAS completed. */
	alloc->Reset();
	{
		ComPtr<ID3D12GraphicsCommandList> list;
		if (FAILED(m_device5->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list))))
			return false;
		if (FAILED(list.As(&buildList)))
			return false;
	}

	if (!BuildTlas(render, buildList.Get(), instances)) {
		printf("[Error] RayTracedShadows: TLAS build failed\n");
		return false;
	}

	buildList->Close();
	{
		ID3D12CommandList* lists[] = { buildList.Get() };
		render->GetQueue()->ExecuteCommandLists(1, lists);
		render->WaitForGpu();
	}

	HRESULT removed = m_device5->GetDeviceRemovedReason();
	if (FAILED(removed)) {
		printf("[Error] RayTracedShadows: device removed after TLAS build (0x%08X)\n",
			(unsigned)removed);
		fflush(stdout);
		FILE* diag = nullptr;
		if (fopen_s(&diag, "rt_full_diag.log", "a") == 0 && diag) {
			fprintf(diag, "[Error] RayTracedShadows: device removed after TLAS (0x%08X) blas=%d inst=%u\n",
				(unsigned)removed, builtBlas, (unsigned)instances.size());
			fclose(diag);
		}
		return false;
	}

	m_ready = true;
	printf("[Info] RayTracedShadows ready: blas=%d instances=%u srv=%u\n",
		builtBlas, (unsigned)instances.size(), m_tlasSrvIndex);
	return true;
}
