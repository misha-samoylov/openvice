#include "Mesh.hpp"
#include "graphics/TextureFactory.h"
#include "graphics/GpuTextureCache.h"
#include "core/GameConfig.h"

#include <d3dcompiler.h>

ID3D12RootSignature* Mesh::s_rootSig = nullptr;
ID3D12PipelineState* Mesh::s_psoOpaque = nullptr;
ID3D12PipelineState* Mesh::s_psoCutout = nullptr;
ID3D12PipelineState* Mesh::s_psoSoft = nullptr;
ID3D12PipelineState* Mesh::s_psoOpaqueCullNone = nullptr;
ID3D12PipelineState* Mesh::s_psoCutoutCullNone = nullptr;
ID3D12PipelineState* Mesh::s_psoSoftCullNone = nullptr;
ID3D12PipelineState* Mesh::s_psoShadow = nullptr;
ID3D12PipelineState* Mesh::s_psoWire = nullptr;
UINT Mesh::s_samplerIndex = UINT_MAX;
UINT Mesh::s_shadowSamplerIndex = UINT_MAX;
int Mesh::s_sharedRefCount = 0;
bool Mesh::s_rtPixelShader = false;
ID3DBlob* Mesh::s_vsBlob = nullptr;
ID3DBlob* Mesh::s_psBlob = nullptr;
ID3DBlob* Mesh::s_shadowPsBlob = nullptr;

static D3D12_RASTERIZER_DESC MakeRaster(D3D12_CULL_MODE cull, bool wire)
{
	D3D12_RASTERIZER_DESC r = {};
	r.FillMode = wire ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	r.CullMode = cull;
	r.FrontCounterClockwise = FALSE;
	r.DepthClipEnable = TRUE;
	return r;
}

static D3D12_BLEND_DESC MakeBlendOpaque(bool alphaToCoverage)
{
	D3D12_BLEND_DESC b = {};
	b.AlphaToCoverageEnable = alphaToCoverage ? TRUE : FALSE;
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_BLEND_DESC MakeBlendSoft()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].BlendEnable = TRUE;
	b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	b.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_DEPTH_STENCIL_DESC MakeDepth(bool write)
{
	D3D12_DEPTH_STENCIL_DESC d = {};
	d.DepthEnable = TRUE;
	d.DepthWriteMask = write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	d.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	return d;
}

static HRESULT CreateMeshPso(
	ID3D12Device* device,
	ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps,
	const D3D12_BLEND_DESC& blend,
	const D3D12_RASTERIZER_DESC& raster,
	const D3D12_DEPTH_STENCIL_DESC& depth,
	DXGI_FORMAT rtvFormat,
	DXGI_FORMAT dsvFormat,
	UINT sampleCount,
	bool depthOnly,
	ID3D12PipelineState** outPso)
{
	D3D12_INPUT_ELEMENT_DESC layout[2] = {};
	layout[0].SemanticName = "POSITION";
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[0].AlignedByteOffset = 0;
	layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	layout[1].SemanticName = "TEXCOORD";
	layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	layout[1].AlignedByteOffset = 12;
	layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = rootSig;
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	if (ps && !depthOnly)
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	else if (ps)
		pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.BlendState = blend;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState = raster;
	pso.DepthStencilState = depth;
	pso.InputLayout = { layout, 2 };
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = depthOnly ? 0 : 1;
	if (!depthOnly)
		pso.RTVFormats[0] = rtvFormat;
	pso.DSVFormat = dsvFormat;
	pso.SampleDesc.Count = sampleCount > 0 ? sampleCount : 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

HRESULT Mesh::EnsureSharedPipeline(DXRender* pRender)
{
	if (s_rootSig)
		return S_OK;

	HRESULT hr = D3DReadFileToBlob(L"vertex_shader.cso", &s_vsBlob);
	if (FAILED(hr)) {
		printf("Error: cannot read compiled vertex shader (0x%08X)\n", (unsigned)hr);
		return hr;
	}
	hr = D3DReadFileToBlob(L"shadow_ps.cso", &s_shadowPsBlob);
	if (FAILED(hr)) {
		printf("Error: cannot read compiled shadow pixel shader (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	/* Master CSM mesh PS (pixel_shader / pixel_shader_nort). */
	s_rtPixelShader = false;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &s_psBlob);
	if (FAILED(hr)) {
		printf("[Warn] pixel_shader.cso missing (0x%08X) — trying pixel_shader_nort.cso\n",
			(unsigned)hr);
	}
#if ENABLE_RT_INLINE_PS
	else if (pRender->SupportsRaytracing()) {
		s_rtPixelShader = true;
	} else {
		printf("[Warn] GPU has no DXR — using non-RT pixel shader\n");
		s_psBlob->Release();
		s_psBlob = nullptr;
	}
#else
	else {
		printf("[Info] Mesh PS: master CSM path (ENABLE_RT_INLINE_PS=0)\n");
	}
#endif

	if (!s_psBlob) {
		hr = D3DReadFileToBlob(L"pixel_shader_nort.cso", &s_psBlob);
		if (FAILED(hr)) {
			printf("Error: cannot read pixel_shader_nort.cso (0x%08X)\n", (unsigned)hr);
			return hr;
		}
		s_rtPixelShader = false;
	}

	D3D12_DESCRIPTOR_RANGE srv0 = {};
	srv0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv0.NumDescriptors = 1;
	srv0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE srv1 = {};
	srv1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv1.NumDescriptors = 1;
	srv1.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE samp0 = {};
	samp0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samp0.NumDescriptors = 1;
	samp0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE samp1 = {};
	samp1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samp1.NumDescriptors = 1;
	samp1.BaseShaderRegister = 1;

	/* t2 = root SRV (TLAS GPU VA) — required for RayQuery; unused by nort PS. */
	D3D12_ROOT_PARAMETER params[6] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &srv0;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &srv1;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	params[3].Descriptor.ShaderRegister = 2;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[4].DescriptorTable.NumDescriptorRanges = 1;
	params[4].DescriptorTable.pDescriptorRanges = &samp0;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[5].DescriptorTable.NumDescriptorRanges = 1;
	params[5].DescriptorTable.pDescriptorRanges = &samp1;
	params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 6;
	rsDesc.pParameters = params;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			printf("Error: mesh root sig: %s\n", (char*)errBlob->GetBufferPointer());
			errBlob->Release();
		}
		return hr;
	}
	hr = pRender->GetDevice()->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&s_rootSig));
	sigBlob->Release();
	if (FAILED(hr)) {
		printf("Error: CreateRootSignature failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	const DXGI_FORMAT rtv = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT sceneDsv = DXGI_FORMAT_D24_UNORM_S8_UINT;
	const DXGI_FORMAT shadowDsv = DXGI_FORMAT_D32_FLOAT;
	const UINT msaa = pRender->GetMSAASampleCount();

	auto createColorPsos = [&](ID3DBlob* ps) -> HRESULT {
		HRESULT e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendOpaque(false), MakeRaster(D3D12_CULL_MODE_FRONT, false), MakeDepth(true),
			rtv, sceneDsv, msaa, false, &s_psoOpaque);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendOpaque(true), MakeRaster(D3D12_CULL_MODE_FRONT, false), MakeDepth(true),
			rtv, sceneDsv, msaa, false, &s_psoCutout);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendSoft(), MakeRaster(D3D12_CULL_MODE_FRONT, false), MakeDepth(false),
			rtv, sceneDsv, msaa, false, &s_psoSoft);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendOpaque(false), MakeRaster(D3D12_CULL_MODE_NONE, false), MakeDepth(true),
			rtv, sceneDsv, msaa, false, &s_psoOpaqueCullNone);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendOpaque(true), MakeRaster(D3D12_CULL_MODE_NONE, false), MakeDepth(true),
			rtv, sceneDsv, msaa, false, &s_psoCutoutCullNone);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendSoft(), MakeRaster(D3D12_CULL_MODE_NONE, false), MakeDepth(false),
			rtv, sceneDsv, msaa, false, &s_psoSoftCullNone);
		if (FAILED(e)) return e;
		e = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, ps,
			MakeBlendOpaque(false), MakeRaster(D3D12_CULL_MODE_NONE, true), MakeDepth(true),
			rtv, sceneDsv, msaa, false, &s_psoWire);
		return e;
	};

	auto releaseColorPsos = [&]() {
		if (s_psoOpaque) { s_psoOpaque->Release(); s_psoOpaque = nullptr; }
		if (s_psoCutout) { s_psoCutout->Release(); s_psoCutout = nullptr; }
		if (s_psoSoft) { s_psoSoft->Release(); s_psoSoft = nullptr; }
		if (s_psoOpaqueCullNone) { s_psoOpaqueCullNone->Release(); s_psoOpaqueCullNone = nullptr; }
		if (s_psoCutoutCullNone) { s_psoCutoutCullNone->Release(); s_psoCutoutCullNone = nullptr; }
		if (s_psoSoftCullNone) { s_psoSoftCullNone->Release(); s_psoSoftCullNone = nullptr; }
		if (s_psoWire) { s_psoWire->Release(); s_psoWire = nullptr; }
	};

	hr = createColorPsos(s_psBlob);
	if (FAILED(hr) && s_rtPixelShader) {
		printf("[Warn] RT mesh PSO failed (0x%08X) — falling back to pixel_shader_nort.cso\n",
			(unsigned)hr);
		releaseColorPsos();
		s_psBlob->Release();
		s_psBlob = nullptr;
		s_rtPixelShader = false;
		hr = D3DReadFileToBlob(L"pixel_shader_nort.cso", &s_psBlob);
		if (FAILED(hr)) {
			printf("Error: cannot read pixel_shader_nort.cso (0x%08X)\n", (unsigned)hr);
			return hr;
		}
		hr = createColorPsos(s_psBlob);
	}
	if (FAILED(hr)) {
		printf("Error: CreateGraphicsPipelineState (color) failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	{
		D3D12_RASTERIZER_DESC shadowR = MakeRaster(D3D12_CULL_MODE_NONE, false);
		shadowR.DepthBias = 16;
		shadowR.DepthBiasClamp = 0.00018f;
		shadowR.SlopeScaledDepthBias = 1.0f;
		hr = CreateMeshPso(pRender->GetDevice(), s_rootSig, s_vsBlob, s_shadowPsBlob,
			MakeBlendOpaque(false), shadowR, MakeDepth(true),
			rtv, shadowDsv, 1, true, &s_psoShadow);
	}
	if (FAILED(hr)) {
		printf("Error: CreateGraphicsPipelineState (shadow) failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	D3D12_SAMPLER_DESC samp = {};
	samp.Filter = D3D12_FILTER_ANISOTROPIC;
	samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.MaxAnisotropy = 16;
	samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	s_samplerIndex = pRender->CreateSampler(samp);
	if (s_samplerIndex == UINT_MAX)
		return E_FAIL;

	D3D12_SAMPLER_DESC cmp = {};
	cmp.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	cmp.AddressU = cmp.AddressV = cmp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	cmp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	cmp.BorderColor[0] = cmp.BorderColor[1] = cmp.BorderColor[2] = cmp.BorderColor[3] = 1.0f;
	s_shadowSamplerIndex = pRender->CreateSampler(cmp);
	if (s_shadowSamplerIndex == UINT_MAX)
		return E_FAIL;

	printf("[Info] Mesh pipeline ready (RT pixel shader=%d)\n", s_rtPixelShader ? 1 : 0);
	return S_OK;
}

void Mesh::ReleaseSharedResources()
{
	if (s_psoOpaque) { s_psoOpaque->Release(); s_psoOpaque = nullptr; }
	if (s_psoCutout) { s_psoCutout->Release(); s_psoCutout = nullptr; }
	if (s_psoSoft) { s_psoSoft->Release(); s_psoSoft = nullptr; }
	if (s_psoOpaqueCullNone) { s_psoOpaqueCullNone->Release(); s_psoOpaqueCullNone = nullptr; }
	if (s_psoCutoutCullNone) { s_psoCutoutCullNone->Release(); s_psoCutoutCullNone = nullptr; }
	if (s_psoSoftCullNone) { s_psoSoftCullNone->Release(); s_psoSoftCullNone = nullptr; }
	if (s_psoShadow) { s_psoShadow->Release(); s_psoShadow = nullptr; }
	if (s_psoWire) { s_psoWire->Release(); s_psoWire = nullptr; }
	if (s_rootSig) { s_rootSig->Release(); s_rootSig = nullptr; }
	if (s_vsBlob) { s_vsBlob->Release(); s_vsBlob = nullptr; }
	if (s_psBlob) { s_psBlob->Release(); s_psBlob = nullptr; }
	if (s_shadowPsBlob) { s_shadowPsBlob->Release(); s_shadowPsBlob = nullptr; }
	s_samplerIndex = UINT_MAX;
	s_shadowSamplerIndex = UINT_MAX;
	s_sharedRefCount = 0;
	s_rtPixelShader = false;
}

void Mesh::Cleanup()
{
	if (m_pVertexBuffer) {
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	if (m_pIndexBuffer) {
		m_pIndexBuffer->Release();
		m_pIndexBuffer = nullptr;
	}
	if (m_ownsTexture && m_texture.resource) {
		m_texture.resource->Release();
	}
	m_texture.resource = nullptr;
	m_texture.srvIndex = UINT_MAX;
	m_ownsTexture = true;
	m_vertexData.clear();

	if (s_sharedRefCount > 0) {
		s_sharedRefCount--;
		if (s_sharedRefCount == 0)
			ReleaseSharedResources();
	}
}

void Mesh::UploadVertices(DXRender* pRender)
{
	if (!pRender || !m_pVertexBuffer || m_vertexData.empty())
		return;

	/* Recreate DEFAULT buffer from CPU data via init upload path. */
	ID3D12Resource* newVb = nullptr;
	if (FAILED(pRender->CreateDefaultBuffer(
		m_vertexData.data(), (UINT64)m_vertexData.size() * sizeof(float), &newVb)))
		return;
	pRender->DeferRelease(m_pVertexBuffer);
	m_pVertexBuffer = newVb;
}

ID3D12PipelineState* Mesh::SelectPso(DXRender* pRender, MeshRenderContext& ctx) const
{
	if (ctx.pass == MESH_PASS_SHADOW)
		return s_psoShadow;
	if (pRender->IsWireframe() || pRender->GetRasterCullMode() == RasterCullMode::Wireframe)
		return s_psoWire;

	const bool cullNone = (pRender->GetRasterCullMode() == RasterCullMode::CullNone);
	switch (pRender->GetBlendPassMode()) {
	case BlendPassMode::Cutout:
		return cullNone ? s_psoCutoutCullNone : s_psoCutout;
	case BlendPassMode::SoftAlpha:
		return cullNone ? s_psoSoftCullNone : s_psoSoft;
	default:
		return cullNone ? s_psoOpaqueCullNone : s_psoOpaque;
	}
}

void Mesh::Render(DXRender* pRender, MeshRenderContext& ctx)
{
	ID3D12GraphicsCommandList* cmd = pRender->GetCommandList();
	ID3D12PipelineState* pso = SelectPso(pRender, ctx);

	cmd->SetGraphicsRootSignature(s_rootSig);
	if (ctx.pso != pso) {
		cmd->SetPipelineState(pso);
		ctx.pso = pso;
	}

	if (ctx.topology != m_primitiveTopology) {
		cmd->IASetPrimitiveTopology(m_primitiveTopology);
		ctx.topology = m_primitiveTopology;
	}

	if (ctx.vb != m_pVertexBuffer) {
		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
		vbv.SizeInBytes = (UINT)(m_vertexData.size() * sizeof(float));
		vbv.StrideInBytes = sizeof(float) * 5;
		cmd->IASetVertexBuffers(0, 1, &vbv);
		ctx.vb = m_pVertexBuffer;
	}

	if (ctx.ib != m_pIndexBuffer) {
		D3D12_INDEX_BUFFER_VIEW ibv = {};
		ibv.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = m_countIndices * sizeof(unsigned int);
		ibv.Format = DXGI_FORMAT_R32_UINT;
		cmd->IASetIndexBuffer(&ibv);
		ctx.ib = m_pIndexBuffer;
	}

	XMMATRIX wvp = XMMatrixMultiply(m_World, ctx.viewProj);
	m_objectConstBuffer.WVP = XMMatrixTranspose(wvp);
	m_objectConstBuffer.World = XMMatrixTranspose(m_World);
	for (int i = 0; i < 4; i++)
		m_objectConstBuffer.LightVP[i] = XMMatrixTranspose(ctx.lightViewProj[i]);
	m_objectConstBuffer.cascadeSplits = ctx.cascadeSplits;
	m_objectConstBuffer.fogColor = ctx.fogColor;
	m_objectConstBuffer.fogStart = ctx.fogStart;
	m_objectConstBuffer.fogEnd = ctx.fogEnd;
	m_objectConstBuffer.receiveShadows =
		(ctx.pass == MESH_PASS_COLOR) ? ctx.receiveShadows : 0.0f;
	if (s_rtPixelShader && ctx.rtAccelVA == 0)
		m_objectConstBuffer.receiveShadows = 0.0f;
	m_objectConstBuffer.shadowBias = ctx.shadowBias;
	m_objectConstBuffer.windTime = ctx.windTime;
	m_objectConstBuffer.windAmount = m_windAmount;
	m_objectConstBuffer.padWindAlign[0] = 0.0f;
	m_objectConstBuffer.padWindAlign[1] = 0.0f;
	m_objectConstBuffer.sunDir = ctx.sunDir;
	m_objectConstBuffer.padSun = 0.0f;

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* cbPtr = pRender->AllocFrameConstants(sizeof(m_objectConstBuffer), &cbAddr);
	if (!cbPtr)
		return;
	memcpy(cbPtr, &m_objectConstBuffer, sizeof(m_objectConstBuffer));
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);

	UINT texSrv = m_texture.srvIndex;
	if (texSrv == UINT_MAX) {
		GpuTextureCache::Instance().EnsureBlack(pRender);
		if (GpuTextureCache::Instance().HasBlack())
			texSrv = GpuTextureCache::Instance().Black().srvIndex;
		else
			return;
	}

	/* Separate root tables — never rewrite a shared scratch descriptor (GPU reads at execute). */
	cmd->SetGraphicsRootDescriptorTable(1, pRender->GetSrvGpu(texSrv));
	/* t1 = CSM Texture2DArray when shadows on; else bind albedo (type-compatible fallback). */
	UINT shadowSrv = (ctx.pass == MESH_PASS_COLOR && ctx.shadowSrvIndex != UINT_MAX)
		? ctx.shadowSrvIndex : texSrv;
	cmd->SetGraphicsRootDescriptorTable(2, pRender->GetSrvGpu(shadowSrv));

	D3D12_GPU_VIRTUAL_ADDRESS tlasVA = ctx.rtAccelVA;
	if (s_rtPixelShader) {
		if (tlasVA == 0)
			tlasVA = pRender->GetFallbackTlasVA();
		if (tlasVA == 0)
			return;
		cmd->SetGraphicsRootShaderResourceView(3, tlasVA);
	} else {
		cmd->SetGraphicsRootShaderResourceView(3, 0);
	}

	UINT samp = (ctx.samplerIndex != UINT_MAX) ? ctx.samplerIndex : s_samplerIndex;
	if (samp == UINT_MAX)
		return;
	cmd->SetGraphicsRootDescriptorTable(4, pRender->GetSamplerGpu(samp));
	UINT shadowSamp = (ctx.pass == MESH_PASS_COLOR && ctx.shadowSamplerIndex != UINT_MAX)
		? ctx.shadowSamplerIndex
		: ((s_shadowSamplerIndex != UINT_MAX) ? s_shadowSamplerIndex : samp);
	cmd->SetGraphicsRootDescriptorTable(5, pRender->GetSamplerGpu(shadowSamp));

	cmd->DrawIndexedInstanced(m_countIndices, 1, 0, 0, 0);
}

void Mesh::SetPosition(float x, float y, float z,
	float scaleX, float scaleY, float scaleZ,
	float rotx, float roty, float rotz, float rotr)
{
	XMVECTOR vector = XMVectorSet(rotx, roty, rotz, rotr);
	XMMATRIX modelRotation = XMMatrixRotationQuaternion(vector);
	XMMATRIX modelScale = XMMatrixScaling(scaleX, scaleY, scaleZ);
	XMMATRIX modelTranslation = XMMatrixTranslation(x, y, z);
	m_World = modelRotation * modelScale * modelTranslation;
}

void Mesh::SetSharedTexture(const GpuTexture& tex)
{
	if (m_ownsTexture && m_texture.resource) {
		m_texture.resource->Release();
		m_texture.resource = nullptr;
		m_texture.srvIndex = UINT_MAX;
	}
	m_texture = tex;
	m_ownsTexture = false;
}

HRESULT Mesh::SetDataDDS(DXRender* pRender, uint8_t* pDataSourceDDS, size_t fileSizeDDS,
	uint32_t width, uint32_t height, uint32_t dxtCompression, uint32_t depth)
{
	(void)depth;
	if (m_ownsTexture && m_texture.resource) {
		m_texture.resource->Release();
		m_texture.resource = nullptr;
		m_texture.srvIndex = UINT_MAX;
	}
	m_ownsTexture = true;
	HRESULT hr = TextureFactory::CreateSrvFromDxt(
		pRender, pDataSourceDDS, fileSizeDDS, width, height, dxtCompression, &m_texture);
	if (FAILED(hr) || !m_texture.Valid()) {
		GpuTextureCache::Instance().EnsureBlack(pRender);
		if (GpuTextureCache::Instance().HasBlack()) {
			m_texture = GpuTextureCache::Instance().Black();
			m_ownsTexture = false;
			return S_OK;
		}
	}
	return hr;
}

HRESULT Mesh::CreateDataBuffer(
	DXRender* pRender,
	float* pVertices,
	int verticesCount,
	unsigned int* pIndices,
	int indicesCount)
{
	m_vertexData.assign(pVertices, pVertices + verticesCount);
	m_indexData.assign(pIndices, pIndices + indicesCount);

	HRESULT hr = pRender->CreateDefaultBuffer(
		m_vertexData.data(), sizeof(float) * verticesCount, &m_pVertexBuffer);
	if (FAILED(hr)) {
		printf("Error: cannot CreateBuffer vertex buffer\n");
		return hr;
	}

	hr = pRender->CreateDefaultBuffer(
		pIndices, sizeof(unsigned int) * indicesCount, &m_pIndexBuffer);
	if (FAILED(hr))
		printf("Error: cannot CreateBuffer index buffer\n");
	return hr;
}

HRESULT Mesh::Init(DXRender* pRender, float* pVertices, int verticesCount,
	unsigned int* pIndices, int indicesCount, D3D_PRIMITIVE_TOPOLOGY topology)
{
	m_hasAlpha = false;
	m_alphaCutout = false;
	m_windAmount = 0.0f;
	m_texture = {};
	m_ownsTexture = true;
	m_pVertexBuffer = nullptr;
	m_pIndexBuffer = nullptr;
	m_countIndices = indicesCount;
	m_primitiveTopology = topology;
	m_World = XMMatrixIdentity();

	HRESULT hr = EnsureSharedPipeline(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create shared pipeline\n");
		return hr;
	}
	s_sharedRefCount++;

	hr = CreateDataBuffer(pRender, pVertices, verticesCount, pIndices, indicesCount);
	if (FAILED(hr))
		printf("Error: cannot create data buffer\n");
	return hr;
}
