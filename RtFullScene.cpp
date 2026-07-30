#include "RtFullScene.h"
#include "RayTracedShadows.h"
#include "core/GameConfig.h"
#include "Model.h"
#include "ShadowMap.h"

#include <stdio.h>
#include <cmath>
#include <d3dcompiler.h>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

struct RtShadeTriCPU
{
	XMFLOAT3 p0; float pad0;
	XMFLOAT3 p1; float pad1;
	XMFLOAT3 p2; float pad2;
	XMFLOAT2 uv0;
	XMFLOAT2 uv1;
	XMFLOAT2 uv2;
	UINT texIndex;
	UINT pad3;
};

/* Must match TLAS instance order. Tris are local-space; rows are world 3x4. */
struct RtInstCPU
{
	UINT triStart;
	UINT triCount;
	UINT pad0;
	UINT pad1;
	XMFLOAT4 row0;
	XMFLOAT4 row1;
	XMFLOAT4 row2;
};

struct RtFullCB
{
	XMFLOAT4X4 InvViewProj;
	XMFLOAT3 CamPos;
	float SeaLevelY;
	XMFLOAT3 SunDir;
	float SunStrength;
	XMFLOAT3 SkyColor;
	float ShadowBias;
	XMFLOAT2 ScreenSize;
	float MaxRayT;
	float Ambient;
	float Time;
	float SunCos;
	XMFLOAT2 Pad1;
};

static_assert(sizeof(RtShadeTriCPU) == 80, "RtShadeTri size");
static_assert(sizeof(RtInstCPU) == 64, "RtInst size");

static XMMATRIX InstanceWorld(const SceneInstance& inst)
{
	return XMMatrixRotationQuaternion(XMVectorSet(
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3])) *
		XMMatrixScaling(inst.scale[0], inst.scale[1], inst.scale[2]) *
		XMMatrixTranslation(inst.x, inst.y, inst.z);
}

static void StoreWorldRows(const XMMATRIX& world, RtInstCPU* out)
{
	/* Same column packing as DXR instance transform / XMVector3Transform. */
	XMFLOAT4X4 m;
	XMStoreFloat4x4(&m, world);
	out->row0 = XMFLOAT4(m._11, m._21, m._31, m._41);
	out->row1 = XMFLOAT4(m._12, m._22, m._32, m._42);
	out->row2 = XMFLOAT4(m._13, m._23, m._33, m._43);
}

static bool BakeMeshLocal(
	Mesh* mesh,
	std::vector<RtShadeTriCPU>& tris,
	UINT* outStart,
	UINT* outCount)
{
	const std::vector<float>& vd = mesh->GetVertexData();
	const std::vector<unsigned int>& id = mesh->GetIndexData();
	if (vd.empty() || id.size() < 3 || id.size() != mesh->GetIndexCount())
		return false;

	*outStart = (UINT)tris.size();
	UINT tex = mesh->GetTextureSrvIndex();
	if (tex == UINT_MAX)
		tex = 0xFFFFFFFFu;

	for (size_t t = 0; t + 2 < id.size(); t += 3) {
		unsigned int i0 = id[t], i1 = id[t + 1], i2 = id[t + 2];
		auto load = [&](unsigned int idx, XMFLOAT3* p, XMFLOAT2* uv) {
			if ((size_t)idx * 5 + 4 >= vd.size()) {
				*p = XMFLOAT3(0, 0, 0);
				*uv = XMFLOAT2(0, 0);
				return;
			}
			p->x = vd[(size_t)idx * 5 + 0];
			p->y = vd[(size_t)idx * 5 + 1];
			p->z = vd[(size_t)idx * 5 + 2];
			uv->x = vd[(size_t)idx * 5 + 3];
			uv->y = vd[(size_t)idx * 5 + 4];
		};
		RtShadeTriCPU tri = {};
		load(i0, &tri.p0, &tri.uv0);
		load(i1, &tri.p1, &tri.uv1);
		load(i2, &tri.p2, &tri.uv2);
		tri.texIndex = tex;
		tris.push_back(tri);
	}

	*outCount = (UINT)tris.size() - *outStart;
	return *outCount > 0;
}

static void AppendShadeMeshes(
	const std::vector<SceneInstance>& instances,
	RayTracedShadows* rt,
	std::vector<RtShadeTriCPU>& tris,
	std::vector<RtInstCPU>& insts,
	std::unordered_map<Mesh*, std::pair<UINT, UINT>>& meshRanges)
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
			if (!mesh || !rt->HasBlas(mesh))
				continue;
			if (mesh->GetIndexCount() < 3 || (mesh->GetIndexCount() % 3) != 0)
				continue;
			if (mesh->GetPrimitiveTopology() != D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
				continue;

			std::unordered_map<Mesh*, std::pair<UINT, UINT>>::iterator it = meshRanges.find(mesh);
			if (it == meshRanges.end()) {
				UINT start = 0, count = 0;
				if (!BakeMeshLocal(mesh, tris, &start, &count))
					continue;
				it = meshRanges.insert(std::make_pair(mesh, std::make_pair(start, count))).first;
			}

			RtInstCPU rec = {};
			rec.triStart = it->second.first;
			rec.triCount = it->second.second;
			StoreWorldRows(world, &rec);
			insts.push_back(rec);
		}
	}
}

HRESULT RtFullScene::Init(DXRender* render)
{
	Cleanup();
	FILE* diag = nullptr;
	fopen_s(&diag, "rt_full_diag.log", "w");
	auto dlog = [&](const char* msg) {
		printf("%s", msg);
		fflush(stdout);
		if (diag) { fputs(msg, diag); fflush(diag); }
	};

	if (!render || !render->SupportsRaytracing()) {
		dlog("[Error] RtFullScene: no DXR\n");
		if (diag) fclose(diag);
		return E_FAIL;
	}

	ID3D12Device* device = render->GetDevice();

	D3D12_DESCRIPTOR_RANGE srvTri = {};
	srvTri.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvTri.NumDescriptors = 1;
	srvTri.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE srvInst = {};
	srvInst.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvInst.NumDescriptors = 1;
	srvInst.BaseShaderRegister = 2;

	D3D12_DESCRIPTOR_RANGE srvTex = {};
	srvTex.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvTex.NumDescriptors = 16384; /* finite bindless range — UINT_MAX can fail serialize */
	srvTex.BaseShaderRegister = 0;
	srvTex.RegisterSpace = 1;
	srvTex.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER params[5] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	params[1].Descriptor.ShaderRegister = 0;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &srvTri;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &srvInst;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[4].DescriptorTable.NumDescriptorRanges = 1;
	params[4].DescriptorTable.pDescriptorRanges = &srvTex;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC staticSamp = {};
	staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamp.MaxLOD = D3D12_FLOAT32_MAX;
	staticSamp.ShaderRegister = 0;
	staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 5;
	rsDesc.pParameters = params;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &staticSamp;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		char buf[512];
		sprintf_s(buf, "[Error] RtFullScene root sig 0x%08X: %s\n", (unsigned)hr,
			errBlob ? (char*)errBlob->GetBufferPointer() : "");
		dlog(buf);
		if (errBlob) errBlob->Release();
		if (diag) fclose(diag);
		return hr;
	}
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr)) {
		char buf[128];
		sprintf_s(buf, "[Error] RtFullScene CreateRootSignature 0x%08X\n", (unsigned)hr);
		dlog(buf);
		if (diag) fclose(diag);
		return hr;
	}

	ID3DBlob* vs = nullptr;
	ID3DBlob* ps = nullptr;
	hr = D3DReadFileToBlob(L"rt_fullscreen_vs.cso", &vs);
	if (FAILED(hr)) {
		dlog("[Error] RtFullScene: rt_fullscreen_vs.cso missing\n");
		if (diag) fclose(diag);
		return hr;
	}
	hr = D3DReadFileToBlob(L"rt_full_ps.cso", &ps);
	if (FAILED(hr)) {
		dlog("[Error] RtFullScene: rt_full_ps.cso missing\n");
		vs->Release();
		if (diag) fclose(diag);
		return hr;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = m_rootSig;
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	pso.DepthStencilState.DepthEnable = FALSE;
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.SampleDesc.Count = 1;
	hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
	vs->Release();
	ps->Release();
	if (FAILED(hr)) {
		char buf[128];
		sprintf_s(buf, "[Error] RtFullScene PSO failed (0x%08X)\n", (unsigned)hr);
		dlog(buf);
		if (diag) fclose(diag);
		return hr;
	}

#if ENABLE_RT_FULL_HALF_RES
	D3D12_DESCRIPTOR_RANGE upSrv = {};
	upSrv.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	upSrv.NumDescriptors = 1;
	upSrv.BaseShaderRegister = 0;
	D3D12_ROOT_PARAMETER upParams[1] = {};
	upParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	upParams[0].DescriptorTable.NumDescriptorRanges = 1;
	upParams[0].DescriptorTable.pDescriptorRanges = &upSrv;
	upParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_STATIC_SAMPLER_DESC upSamp = staticSamp;
	upSamp.AddressU = upSamp.AddressV = upSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	D3D12_ROOT_SIGNATURE_DESC upRs = {};
	upRs.NumParameters = 1;
	upRs.pParameters = upParams;
	upRs.NumStaticSamplers = 1;
	upRs.pStaticSamplers = &upSamp;
	ID3DBlob* upSig = nullptr;
	hr = D3D12SerializeRootSignature(&upRs, D3D_ROOT_SIGNATURE_VERSION_1, &upSig, &errBlob);
	if (FAILED(hr)) {
		dlog("[Error] RtFullScene upsample root sig failed\n");
		if (errBlob) errBlob->Release();
		if (diag) fclose(diag);
		return hr;
	}
	hr = device->CreateRootSignature(0, upSig->GetBufferPointer(), upSig->GetBufferSize(),
		IID_PPV_ARGS(&m_upRootSig));
	upSig->Release();
	if (FAILED(hr)) {
		if (diag) fclose(diag);
		return hr;
	}

	ID3DBlob* upVs = nullptr;
	ID3DBlob* upPs = nullptr;
	hr = D3DReadFileToBlob(L"rt_fullscreen_vs.cso", &upVs);
	if (SUCCEEDED(hr))
		hr = D3DReadFileToBlob(L"rt_upsample_ps.cso", &upPs);
	if (FAILED(hr)) {
		dlog("[Error] RtFullScene: upsample shaders missing\n");
		if (upVs) upVs->Release();
		if (diag) fclose(diag);
		return hr;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC upPso = pso;
	upPso.pRootSignature = m_upRootSig;
	upPso.VS = { upVs->GetBufferPointer(), upVs->GetBufferSize() };
	upPso.PS = { upPs->GetBufferPointer(), upPs->GetBufferSize() };
	hr = device->CreateGraphicsPipelineState(&upPso, IID_PPV_ARGS(&m_upPso));
	upVs->Release();
	upPs->Release();
	if (FAILED(hr)) {
		dlog("[Error] RtFullScene upsample PSO failed\n");
		if (diag) fclose(diag);
		return hr;
	}
	if (FAILED(CreateHalfTargets(render))) {
		dlog("[Error] RtFullScene half-res targets failed\n");
		if (diag) fclose(diag);
		return E_FAIL;
	}
	dlog("[Info] RtFullScene pipeline ready (primary + sun shadow, half-res)\n");
#else
	dlog("[Info] RtFullScene pipeline ready (primary + sun shadow)\n");
#endif
	if (diag) fclose(diag);
	return S_OK;
}

void RtFullScene::ReleaseHalfTargets(DXRender* render)
{
	if (m_halfTex) {
		if (render)
			render->DeferRelease(m_halfTex);
		else
			m_halfTex->Release();
		m_halfTex = nullptr;
	}
	m_halfRtv = m_halfSrv = UINT_MAX;
	m_halfW = m_halfH = m_fullW = m_fullH = 0;
	m_halfState = D3D12_RESOURCE_STATE_COMMON;
}

HRESULT RtFullScene::CreateHalfTargets(DXRender* render)
{
	ReleaseHalfTargets(render);
	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	HRESULT hr = render->CreateTexture2D(
		m_halfW, m_halfH, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_halfTex);
	if (FAILED(hr))
		return hr;
	m_halfState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_halfRtv = render->AllocRtvIndex();
	if (m_halfRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_halfTex, nullptr, render->GetRtvCpu(m_halfRtv));
	m_halfSrv = render->CreateTextureSrv(m_halfTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	return (m_halfSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void RtFullScene::Cleanup()
{
	m_ready = false;
	m_triBuffer.Reset();
	m_instBuffer.Reset();
	m_triSrv = m_instSrv = UINT_MAX;
	m_triCount = m_instCount = 0;
	ReleaseHalfTargets(nullptr);
	if (m_upPso) { m_upPso->Release(); m_upPso = nullptr; }
	if (m_upRootSig) { m_upRootSig->Release(); m_upRootSig = nullptr; }
	if (m_pso) { m_pso->Release(); m_pso = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
}

bool RtFullScene::BuildShadeData(DXRender* render, const Scene& scene, RayTracedShadows* rt)
{
	m_ready = false;
	m_triBuffer.Reset();
	m_instBuffer.Reset();
	m_triSrv = m_instSrv = UINT_MAX;

	FILE* diag = nullptr;
	fopen_s(&diag, "rt_full_diag.log", "a");
	auto dlog = [&](const char* msg) {
		printf("%s", msg);
		fflush(stdout);
		if (diag) { fputs(msg, diag); fflush(diag); }
	};

	if (!rt || !rt->IsReady()) {
		dlog("[Error] RtFullScene: TLAS not ready for shade bake\n");
		if (diag) fclose(diag);
		return false;
	}

	std::vector<RtShadeTriCPU> tris;
	std::vector<RtInstCPU> insts;
	std::unordered_map<Mesh*, std::pair<UINT, UINT>> meshRanges;
	tris.reserve(256 * 1024);
	insts.reserve(65536);
	AppendShadeMeshes(scene.Opaque(), rt, tris, insts, meshRanges);
	AppendShadeMeshes(scene.Alpha(), rt, tris, insts, meshRanges);
	if (tris.empty() || insts.empty()) {
		char buf[256];
		sprintf_s(buf, "[Error] RtFullScene: no shade triangles (opaque=%zu alpha=%zu)\n",
			scene.Opaque().size(), scene.Alpha().size());
		dlog(buf);
		if (diag) fclose(diag);
		return false;
	}

	const UINT64 triBytes = (UINT64)tris.size() * sizeof(RtShadeTriCPU);
	const UINT64 instBytes = (UINT64)insts.size() * sizeof(RtInstCPU);

	{
		char buf[192];
		sprintf_s(buf, "[Info] RtFullScene baking uniqueTris=%zu bytes=%llu insts=%zu meshes=%zu\n",
			tris.size(), (unsigned long long)triBytes, insts.size(), meshRanges.size());
		dlog(buf);
	}

	/* Use UPLOAD heaps directly — avoids init-upload list after TLAS WaitForGpu
	 * (CreateDefaultBuffer was returning DXGI_ERROR_DEVICE_REMOVED). */
	auto createUploadSrv = [&](const void* data, UINT64 bytes, UINT count, UINT stride,
		ComPtr<ID3D12Resource>* outBuf, UINT* outSrv) -> bool {
		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = bytes;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> buf;
		HRESULT hr = render->GetDevice()->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&buf));
		if (FAILED(hr)) {
			char err[128];
			sprintf_s(err, "[Error] RtFullScene upload CreateCommitted 0x%08X\n", (unsigned)hr);
			dlog(err);
			return false;
		}
		void* mapped = nullptr;
		hr = buf->Map(0, nullptr, &mapped);
		if (FAILED(hr) || !mapped) {
			dlog("[Error] RtFullScene upload Map failed\n");
			return false;
		}
		memcpy(mapped, data, (size_t)bytes);
		buf->Unmap(0, nullptr);
		UINT srv = render->CreateTypedBufferSrv(buf.Get(), count, stride);
		if (srv == UINT_MAX) {
			dlog("[Error] RtFullScene shade SRV create failed\n");
			return false;
		}
		*outBuf = buf;
		*outSrv = srv;
		return true;
	};

	if (!createUploadSrv(tris.data(), triBytes, (UINT)tris.size(), sizeof(RtShadeTriCPU),
			&m_triBuffer, &m_triSrv)) {
		if (diag) fclose(diag);
		return false;
	}
	if (!createUploadSrv(insts.data(), instBytes, (UINT)insts.size(), sizeof(RtInstCPU),
			&m_instBuffer, &m_instSrv)) {
		if (diag) fclose(diag);
		return false;
	}

	m_triCount = (UINT)tris.size();
	m_instCount = (UINT)insts.size();
	m_ready = true;
	char buf[128];
	sprintf_s(buf, "[Info] RtFullScene shade data: tris=%u instances=%u\n", m_triCount, m_instCount);
	dlog(buf);
	if (diag) fclose(diag);
	return true;
}

void RtFullScene::DrawTrace(
	DXRender* render,
	D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddr)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	render->BindDescriptorHeaps();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(m_pso);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->SetGraphicsRootShaderResourceView(1, tlasVA);
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(m_triSrv));
	cmd->SetGraphicsRootDescriptorTable(3, render->GetSrvGpu(m_instSrv));
	cmd->SetGraphicsRootDescriptorTable(4, render->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void RtFullScene::DrawUpsample(DXRender* render)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	render->BindDescriptorHeaps();
	cmd->SetGraphicsRootSignature(m_upRootSig);
	cmd->SetPipelineState(m_upPso);
	cmd->SetGraphicsRootDescriptorTable(0, render->GetSrvGpu(m_halfSrv));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void RtFullScene::Apply(
	DXRender* render,
	Camera* camera,
	FXMVECTOR sunDirToward,
	D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
	float seaLevelY,
	float timeSec)
{
	if (!m_ready || !m_pso || tlasVA == 0)
		return;

	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, XMMatrixMultiply(view, proj));

	RtFullCB cb = {};
	XMStoreFloat4x4(&cb.InvViewProj, XMMatrixTranspose(invViewProj));
	XMVECTOR cam = camera->GetPosition();
	cb.CamPos = XMFLOAT3(XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam));
	cb.SeaLevelY = seaLevelY;
	XMStoreFloat3(&cb.SunDir, XMVector3Normalize(sunDirToward));
	cb.SunStrength = 1.25f;
	cb.SkyColor = XMFLOAT3(SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B);
	cb.ShadowBias = 0.1f;
	cb.MaxRayT = DRAW_DISTANCE;
	cb.Ambient = 0.18f;
	cb.Time = timeSec;
	/* ~0.55° sun angular radius → cos(theta). */
	cb.SunCos = 0.9996f;

#if ENABLE_RT_FULL_HALF_RES
	if (!m_halfTex || m_fullW != render->GetBackBufferWidth() ||
		m_fullH != render->GetBackBufferHeight()) {
		if (FAILED(CreateHalfTargets(render)))
			return;
	}
	cb.ScreenSize = XMFLOAT2((float)m_halfW, (float)m_halfH);
#else
	cb.ScreenSize = XMFLOAT2((float)render->GetBackBufferWidth(), (float)render->GetBackBufferHeight());
#endif

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

#if ENABLE_RT_FULL_HALF_RES
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	if (m_halfState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_halfTex, m_halfState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_halfState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = render->GetRtvCpu(m_halfRtv);
	cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	D3D12_VIEWPORT vp = {};
	vp.Width = (float)m_halfW;
	vp.Height = (float)m_halfH;
	vp.MaxDepth = 1.0f;
	D3D12_RECT sc = { 0, 0, (LONG)m_halfW, (LONG)m_halfH };
	cmd->RSSetViewports(1, &vp);
	cmd->RSSetScissorRects(1, &sc);
	const float clear[4] = { SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B, 1.0f };
	cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
	DrawTrace(render, tlasVA, cbAddr);

	render->Transition(m_halfTex, m_halfState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_halfState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	render->BindBackBufferOnly();
	DrawUpsample(render);
#else
	render->BindBackBufferOnly();
	DrawTrace(render, tlasVA, cbAddr);
#endif

	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
