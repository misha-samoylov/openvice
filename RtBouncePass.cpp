#include "RtBouncePass.h"
#include "RayTracedShadows.h"
#include "core/GameConfig.h"
#include "Model.h"

#include <stdio.h>
#include <cmath>
#include <d3dcompiler.h>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

struct RtBounceCB
{
	XMFLOAT4X4 InvViewProj;
	XMFLOAT4X4 ViewProj;
	XMFLOAT3 CamPos;
	float BounceStrength;
	XMFLOAT3 SunDir;
	float SunStrength;
	XMFLOAT3 SkyColor;
	float ShadowBias;
	float MaxRayT;
	float AoRadius;
	float ReflectStrength;
	float AoStrength;
};

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
				meshRanges[mesh] = std::make_pair(start, count);
				it = meshRanges.find(mesh);
			}

			RtInstCPU ri = {};
			ri.triStart = it->second.first;
			ri.triCount = it->second.second;
			StoreWorldRows(world, &ri);
			insts.push_back(ri);
		}
	}
}

static HRESULT CreateFullscreenPso(
	ID3D12Device* device, ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps, DXGI_FORMAT rtvFmt,
	ID3D12PipelineState** outPso)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = rootSig;
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
	pso.RTVFormats[0] = rtvFmt;
	pso.SampleDesc.Count = 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

HRESULT RtBouncePass::CreateTargets(DXRender* render)
{
	ReleaseTargets(render);

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_rtW = m_fullW > 0 ? m_fullW : 1;
	m_rtH = m_fullH > 0 ? m_fullH : 1;

	HRESULT hr = render->CreateTexture2D(
		m_fullW, m_fullH, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST,
		&m_colorTex);
	if (FAILED(hr))
		return hr;
	m_colorState = D3D12_RESOURCE_STATE_COPY_DEST;
	m_colorSrv = render->CreateTextureSrv(m_colorTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	if (m_colorSrv == UINT_MAX)
		return E_FAIL;

	hr = render->CreateTexture2D(
		m_rtW, m_rtH, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_rtTex);
	if (FAILED(hr))
		return hr;
	m_rtState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_rtRtv = render->AllocRtvIndex();
	if (m_rtRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_rtTex, nullptr, render->GetRtvCpu(m_rtRtv));
	m_rtSrv = render->CreateTextureSrv(m_rtTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	return (m_rtSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void RtBouncePass::ReleaseTargets(DXRender* render)
{
	if (m_rtTex) {
		if (render)
			render->DeferRelease(m_rtTex);
		else
			m_rtTex->Release();
		m_rtTex = nullptr;
	}
	if (m_colorTex) {
		if (render)
			render->DeferRelease(m_colorTex);
		else
			m_colorTex->Release();
		m_colorTex = nullptr;
	}
	m_colorSrv = m_rtRtv = m_rtSrv = UINT_MAX;
}

void RtBouncePass::ReleaseShade()
{
	m_shadeReady = false;
	m_triBuffer.Reset();
	m_instBuffer.Reset();
	m_triSrv = m_instSrv = UINT_MAX;
}

HRESULT RtBouncePass::Init(DXRender* render)
{
	Cleanup();
	if (!render || !render->SupportsRaytracing()) {
		printf("[Warn] RtBouncePass: no DXR — skipped\n");
		return E_FAIL;
	}

	ID3D12Device* device = render->GetDevice();

	/* Trace root: b0 CB, t0 color, t1 depth, t2 TLAS, t3 tris, t4 insts, t0 space1 bindless, s0/s1. */
	D3D12_DESCRIPTOR_RANGE srvColor = {};
	srvColor.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvColor.NumDescriptors = 1;
	srvColor.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE srvDepth = {};
	srvDepth.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvDepth.NumDescriptors = 1;
	srvDepth.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE srvTri = {};
	srvTri.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvTri.NumDescriptors = 1;
	srvTri.BaseShaderRegister = 3;

	D3D12_DESCRIPTOR_RANGE srvInst = {};
	srvInst.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvInst.NumDescriptors = 1;
	srvInst.BaseShaderRegister = 4;

	D3D12_DESCRIPTOR_RANGE srvTex = {};
	srvTex.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvTex.NumDescriptors = 16384;
	srvTex.BaseShaderRegister = 0;
	srvTex.RegisterSpace = 1;
	srvTex.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER params[7] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &srvColor;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &srvDepth;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	params[3].Descriptor.ShaderRegister = 2;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[4].DescriptorTable.NumDescriptorRanges = 1;
	params[4].DescriptorTable.pDescriptorRanges = &srvTri;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[5].DescriptorTable.NumDescriptorRanges = 1;
	params[5].DescriptorTable.pDescriptorRanges = &srvInst;
	params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[6].DescriptorTable.NumDescriptorRanges = 1;
	params[6].DescriptorTable.pDescriptorRanges = &srvTex;
	params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	/* Samplers via static for LinSamp + dynamic point — use static both for simplicity. */
	D3D12_STATIC_SAMPLER_DESC staticSamp[2] = {};
	staticSamp[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamp[0].AddressU = staticSamp[0].AddressV = staticSamp[0].AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamp[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamp[0].ShaderRegister = 0;
	staticSamp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	staticSamp[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamp[1].AddressU = staticSamp[1].AddressV = staticSamp[1].AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamp[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamp[1].ShaderRegister = 1;
	staticSamp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 7;
	rsDesc.pParameters = params;
	rsDesc.NumStaticSamplers = 2;
	rsDesc.pStaticSamplers = staticSamp;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			printf("[Error] RtBouncePass root sig: %s\n", (char*)errBlob->GetBufferPointer());
			errBlob->Release();
		}
		return hr;
	}
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	/* Composite: simpler — color + rt shade factor. */
	D3D12_DESCRIPTOR_RANGE c0 = {};
	c0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	c0.NumDescriptors = 1;
	c0.BaseShaderRegister = 0;
	D3D12_DESCRIPTOR_RANGE c1 = {};
	c1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	c1.NumDescriptors = 1;
	c1.BaseShaderRegister = 1;
	D3D12_DESCRIPTOR_RANGE cSamp = {};
	cSamp.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	cSamp.NumDescriptors = 1;
	cSamp.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER cparams[3] = {};
	cparams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	cparams[0].DescriptorTable.NumDescriptorRanges = 1;
	cparams[0].DescriptorTable.pDescriptorRanges = &c0;
	cparams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	cparams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	cparams[1].DescriptorTable.NumDescriptorRanges = 1;
	cparams[1].DescriptorTable.pDescriptorRanges = &c1;
	cparams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	cparams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	cparams[2].DescriptorTable.NumDescriptorRanges = 1;
	cparams[2].DescriptorTable.pDescriptorRanges = &cSamp;
	cparams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC crs = {};
	crs.NumParameters = 3;
	crs.pParameters = cparams;
	hr = D3D12SerializeRootSignature(&crs, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_compRootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	ID3DBlob* vs = nullptr;
	ID3DBlob* psTrace = nullptr;
	ID3DBlob* psComp = nullptr;
	hr = D3DReadFileToBlob(L"rt_fullscreen_vs.cso", &vs);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_fullscreen_vs.cso missing\n");
		return hr;
	}
	hr = D3DReadFileToBlob(L"rt_bounce_ps.cso", &psTrace);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_bounce_ps.cso missing\n");
		vs->Release();
		return hr;
	}
	hr = D3DReadFileToBlob(L"rt_bounce_composite_ps.cso", &psComp);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_bounce_composite_ps.cso missing\n");
		vs->Release();
		psTrace->Release();
		return hr;
	}

	hr = CreateFullscreenPso(device, m_rootSig, vs, psTrace, DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoTrace);
	if (SUCCEEDED(hr))
		hr = CreateFullscreenPso(device, m_compRootSig, vs, psComp, DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoComposite);
	vs->Release();
	psTrace->Release();
	psComp->Release();
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass PSO failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_linearSampler = render->CreateSampler(sd);
	m_pointSampler = m_linearSampler;
	if (m_linearSampler == UINT_MAX)
		return E_FAIL;

	hr = CreateTargets(render);
	if (FAILED(hr))
		return hr;

	printf("[Info] RT sun shadows + RTAO ready (%ux%u)\n", m_rtW, m_rtH);
	return S_OK;
}

void RtBouncePass::Cleanup()
{
	ReleaseShade();
	ReleaseTargets(nullptr);
	if (m_psoComposite) { m_psoComposite->Release(); m_psoComposite = nullptr; }
	if (m_psoTrace) { m_psoTrace->Release(); m_psoTrace = nullptr; }
	if (m_compRootSig) { m_compRootSig->Release(); m_compRootSig = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
	m_pointSampler = m_linearSampler = UINT_MAX;
}

bool RtBouncePass::BuildShadeData(DXRender* render, const Scene& scene, RayTracedShadows* rt)
{
	ReleaseShade();
	if (!rt || !rt->IsReady()) {
		printf("[Error] RtBouncePass: TLAS not ready for shade bake\n");
		return false;
	}

	std::vector<RtShadeTriCPU> tris;
	std::vector<RtInstCPU> insts;
	std::unordered_map<Mesh*, std::pair<UINT, UINT>> meshRanges;
	tris.reserve(256 * 1024);
	insts.reserve(65536);

	/* Must match RayTracedShadows::AppendInstances order exactly (InstanceID). */
	AppendShadeMeshes(scene.Opaque(), rt, tris, insts, meshRanges);
	AppendShadeMeshes(scene.Alpha(), rt, tris, insts, meshRanges);

	if (tris.empty() || insts.empty()) {
		printf("[Error] RtBouncePass: no shade triangles\n");
		return false;
	}

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
		if (FAILED(hr))
			return false;
		void* mapped = nullptr;
		if (FAILED(buf->Map(0, nullptr, &mapped)) || !mapped)
			return false;
		memcpy(mapped, data, (size_t)bytes);
		buf->Unmap(0, nullptr);
		UINT srv = render->CreateTypedBufferSrv(buf.Get(), count, stride);
		if (srv == UINT_MAX)
			return false;
		*outBuf = buf;
		*outSrv = srv;
		return true;
	};

	const UINT64 triBytes = (UINT64)tris.size() * sizeof(RtShadeTriCPU);
	const UINT64 instBytes = (UINT64)insts.size() * sizeof(RtInstCPU);
	if (!createUploadSrv(tris.data(), triBytes, (UINT)tris.size(), sizeof(RtShadeTriCPU),
			&m_triBuffer, &m_triSrv))
		return false;
	if (!createUploadSrv(insts.data(), instBytes, (UINT)insts.size(), sizeof(RtInstCPU),
			&m_instBuffer, &m_instSrv))
		return false;

	m_shadeReady = true;
	printf("[Info] RT shadow shade data: tris=%zu instances=%zu (alpha cutout enabled)\n",
		tris.size(), insts.size());
	return true;
}

void RtBouncePass::DrawTrace(DXRender* render, D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddr, UINT depthSrv)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	render->BindDescriptorHeaps();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(m_psoTrace);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(m_colorSrv));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(depthSrv));
	cmd->SetGraphicsRootShaderResourceView(3, tlasVA);
	cmd->SetGraphicsRootDescriptorTable(4, render->GetSrvGpu(m_triSrv));
	cmd->SetGraphicsRootDescriptorTable(5, render->GetSrvGpu(m_instSrv));
	cmd->SetGraphicsRootDescriptorTable(6, render->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void RtBouncePass::DrawComposite(DXRender* render, D3D12_GPU_VIRTUAL_ADDRESS /*tlasVA*/)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	render->BindDescriptorHeaps();
	cmd->SetGraphicsRootSignature(m_compRootSig);
	cmd->SetPipelineState(m_psoComposite);
	cmd->SetGraphicsRootDescriptorTable(0, render->GetSrvGpu(m_colorSrv));
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(m_rtSrv));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSamplerGpu(m_linearSampler));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void RtBouncePass::Apply(
	DXRender* render,
	Camera* camera,
	FXMVECTOR sunDirToward,
	D3D12_GPU_VIRTUAL_ADDRESS tlasVA,
	bool enableShadows,
	bool enableRtao)
{
	if (!m_rootSig || !m_psoTrace || !m_colorTex || tlasVA == 0 || !m_shadeReady)
		return;

	UINT depthSrv = render->GetDepthSrvIndex();
	ID3D12Resource* backBuf = render->GetBackBuffer();
	if (depthSrv == UINT_MAX || !backBuf)
		return;

	if (m_fullW != render->GetBackBufferWidth() || m_fullH != render->GetBackBufferHeight()) {
		if (FAILED(CreateTargets(render)))
			return;
	}

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();

	render->Transition(backBuf, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	if (m_colorState != D3D12_RESOURCE_STATE_COPY_DEST) {
		render->Transition(m_colorTex, m_colorState, D3D12_RESOURCE_STATE_COPY_DEST);
		m_colorState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	cmd->CopyResource(m_colorTex, backBuf);
	render->Transition(backBuf, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	render->Transition(m_colorTex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_colorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	RtBounceCB cb = {};
	XMStoreFloat4x4(&cb.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&cb.ViewProj, XMMatrixTranspose(viewProj));
	XMVECTOR cam = camera->GetPosition();
	cb.CamPos = XMFLOAT3(XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam));
	XMStoreFloat3(&cb.SunDir, XMVector3Normalize(sunDirToward));
	/* SunStrength>0.5 enables soft RT sun shadows in the shader. */
	cb.SunStrength = enableShadows ? 1.0f : 0.0f;
	cb.SkyColor = XMFLOAT3(SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B);
	cb.ShadowBias = 0.08f;
	/* Shadow rays travel farther than mesh draw distance so distant casters still shade. */
	cb.MaxRayT = DRAW_DISTANCE * 2.0f;
	cb.AoRadius = 3.0f;
	cb.AoStrength = enableRtao ? 0.75f : 0.0f;
	cb.BounceStrength = 0.0f;
	cb.ReflectStrength = 0.0f;

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

	if (m_rtState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_rtTex, m_rtState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_rtState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_VIEWPORT rtVP = {};
	rtVP.Width = (float)m_rtW;
	rtVP.Height = (float)m_rtH;
	rtVP.MaxDepth = 1.0f;
	D3D12_RECT rtSc = { 0, 0, (LONG)m_rtW, (LONG)m_rtH };
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = render->GetRtvCpu(m_rtRtv);
	float clearRt[4] = { 0, 0, 0, 0 };
	cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	cmd->ClearRenderTargetView(rtv, clearRt, 0, nullptr);
	cmd->RSSetViewports(1, &rtVP);
	cmd->RSSetScissorRects(1, &rtSc);
	DrawTrace(render, tlasVA, cbAddr, depthSrv);

	render->Transition(m_rtTex, m_rtState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_rtState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	render->BindBackBufferOnly();
	DrawComposite(render, tlasVA);

	/* Stay on resolved back buffer (GodRays/PostFX follow); do not rebind MSAA. */
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
