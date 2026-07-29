#include "Water.h"
#include "graphics/TextureFactory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <d3dcompiler.h>

#include "renderware.h"

#define WATER_X_OFFSET 400.0f
#define WATER_Z_OFFSET 0.5f
#define SMALL_SECTOR_SIZE 32.0f

static float WaterFromSmallX(int x)
{
	return (float)(x - 64) * SMALL_SECTOR_SIZE;
}

static float WaterFromSmallY(int y)
{
	return (float)(y - 64) * SMALL_SECTOR_SIZE;
}

bool Water::LoadWaterPro(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		printf("[Error] Cannot open %s\n", path);
		return false;
	}

	fread(&m_numLevels, sizeof(m_numLevels), 1, f);
	fread(m_waterZs, sizeof(m_waterZs), 1, f);

	float rects[MAX_LEVELS * 4];
	fread(rects, sizeof(rects), 1, f);

	fread(m_largeBlocks, sizeof(m_largeBlocks), 1, f);
	fread(m_fineBlocks, sizeof(m_fineBlocks), 1, f);
	fclose(f);

	if (m_numLevels < 0 || m_numLevels > MAX_LEVELS) {
		printf("[Error] Invalid water level count: %d\n", m_numLevels);
		return false;
	}

	printf("[Info] Loaded waterpro.dat levels=%d\n", m_numLevels);
	return true;
}

bool Water::LoadWaterTexture(DXRender* render, const char* particleTxdPath)
{
	FILE* f = fopen(particleTxdPath, "rb");
	if (!f) {
		printf("[Error] Cannot open %s\n", particleTxdPath);
		return false;
	}

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buffer = (char*)malloc(fileSize);
	if (!buffer) {
		fclose(f);
		return false;
	}
	fread(buffer, 1, fileSize, f);
	fclose(f);

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(buffer, &offset);

	NativeTexture* waterTex = nullptr;
	for (size_t i = 0; i < txd.texList.size(); i++) {
		if (_stricmp(txd.texList[i].name, "waterclear256") == 0) {
			waterTex = &txd.texList[i];
			break;
		}
	}

	if (!waterTex || waterTex->texels.empty()) {
		printf("[Error] waterclear256 not found in particle.txd\n");
		free(buffer);
		return false;
	}

	HRESULT hr = TextureFactory::CreateSrvFromDxt(
		render,
		waterTex->texels[0],
		waterTex->dataSizes[0],
		waterTex->width[0],
		waterTex->height[0],
		waterTex->dxtCompression,
		&m_texture
	);

	if (FAILED(hr)) {
		printf("[Error] Failed to create water texture SRV\n");
		free(buffer);
		return false;
	}

	uint32_t tw = waterTex->width[0];
	uint32_t th = waterTex->height[0];
	free(buffer);

	printf("[Info] Loaded water texture waterclear256 %ux%u\n", tw, th);
	return true;
}

void Water::AddQuad(std::vector<WaterVertex>& verts, std::vector<uint32_t>& indices,
	float gx, float gy, float gz, float size, float uvScale)
{
	uint32_t base = (uint32_t)verts.size();

	WaterVertex v0 = { gx,        gz, gy,        0.0f,     0.0f };
	WaterVertex v1 = { gx,        gz, gy + size, 0.0f,     uvScale };
	WaterVertex v2 = { gx + size, gz, gy + size, uvScale,  uvScale };
	WaterVertex v3 = { gx + size, gz, gy,        uvScale,  0.0f };

	verts.push_back(v0);
	verts.push_back(v1);
	verts.push_back(v2);
	verts.push_back(v3);

	indices.push_back(base + 0);
	indices.push_back(base + 2);
	indices.push_back(base + 1);
	indices.push_back(base + 0);
	indices.push_back(base + 3);
	indices.push_back(base + 2);
}

bool Water::BuildCoastMesh(DXRender* render)
{
	std::vector<WaterVertex> verts;
	std::vector<uint32_t> indices;
	verts.reserve(65536);
	indices.reserve(98304);

	for (int x = 0; x < SMALL_SECTORS; x++) {
		for (int y = 0; y < SMALL_SECTORS; y++) {
			int8_t idx = m_fineBlocks[x][y];
			if (idx < 0 || idx >= m_numLevels)
				continue;

			float gx = WaterFromSmallX(x) - WATER_X_OFFSET;
			float gy = WaterFromSmallY(y);
			float gz = m_waterZs[idx] - WATER_Z_OFFSET;

			AddQuad(verts, indices, gx, gy, gz, SMALL_SECTOR_SIZE, 1.0f);
		}
	}

	(void)m_largeBlocks;

	if (verts.empty()) {
		printf("[Error] No water quads generated\n");
		return false;
	}

	m_coastVertexBytes = (UINT)(sizeof(WaterVertex) * verts.size());
	HRESULT hr = render->CreateDefaultBuffer(verts.data(), m_coastVertexBytes, &m_vb);
	if (FAILED(hr)) {
		printf("[Error] Cannot create water VB\n");
		return false;
	}

	hr = render->CreateDefaultBuffer(
		indices.data(), sizeof(uint32_t) * indices.size(), &m_ib);
	if (FAILED(hr)) {
		printf("[Error] Cannot create water IB\n");
		return false;
	}

	m_coastIndexCount = (UINT)indices.size();
	printf("[Info] Coast water mesh: %u quads, %u indices\n",
		(unsigned)(verts.size() / 4), m_coastIndexCount);
	return true;
}

bool Water::CreateOceanBuffers(DXRender* render)
{
	std::vector<uint32_t> indices;
	indices.reserve(MAX_OCEAN_QUADS * 6);
	for (uint32_t q = 0; q < (uint32_t)MAX_OCEAN_QUADS; q++) {
		uint32_t base = q * 4;
		indices.push_back(base + 0);
		indices.push_back(base + 2);
		indices.push_back(base + 1);
		indices.push_back(base + 0);
		indices.push_back(base + 3);
		indices.push_back(base + 2);
	}

	HRESULT hr = render->CreateDefaultBuffer(
		indices.data(), sizeof(uint32_t) * indices.size(), &m_oceanIB);
	return SUCCEEDED(hr);
}

void Water::ComputeSeaLevel()
{
	int counts[MAX_LEVELS];
	memset(counts, 0, sizeof(counts));

	for (int x = 0; x < SMALL_SECTORS; x++) {
		for (int y = 0; y < SMALL_SECTORS; y++) {
			int8_t idx = m_fineBlocks[x][y];
			if (idx >= 0 && idx < m_numLevels)
				counts[idx]++;
		}
	}

	int best = 0;
	for (int i = 1; i < m_numLevels; i++) {
		if (counts[i] > counts[best])
			best = i;
	}

	m_seaZ = (m_numLevels > 0) ? m_waterZs[best] : 6.0f;
	printf("[Info] Ocean sea level Z=%.3f (level %d)\n", m_seaZ, best);
}

bool Water::IsInsideMapWaterGrid(float gtaX, float gtaY) const
{
	float unsignedX = (gtaX + WATER_X_OFFSET) + 2048.0f;
	float unsignedY = gtaY + 2048.0f;
	int fx = (int)floorf(unsignedX / SMALL_SECTOR_SIZE);
	int fy = (int)floorf(unsignedY / SMALL_SECTOR_SIZE);
	return fx >= 0 && fx < SMALL_SECTORS && fy >= 0 && fy < SMALL_SECTORS;
}

bool Water::CreatePipeline(DXRender* render)
{
	HRESULT hr;
	ID3D12Device* device = render->GetDevice();

	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"water_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] Cannot read water_vs.cso\n");
		return false;
	}

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"water_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Cannot read water_ps.cso\n");
		vsBlob->Release();
		return false;
	}

	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE sampRange = {};
	sampRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	sampRange.NumDescriptors = 1;
	sampRange.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER params[3] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &srvRange;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &sampRange;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 3;
	rsDesc.pParameters = params;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr)) {
		vsBlob->Release();
		psBlob->Release();
		return false;
	}
	hr = device->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr)) {
		vsBlob->Release();
		psBlob->Release();
		return false;
	}

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
	pso.pRootSignature = m_rootSig;
	pso.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	pso.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	pso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	pso.DepthStencilState.DepthEnable = TRUE;
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso.InputLayout = { layout, 2 };
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso.SampleDesc.Count = 1;

	hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
	vsBlob->Release();
	psBlob->Release();
	if (FAILED(hr))
		return false;

	D3D12_SAMPLER_DESC samp = {};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	m_samplerIndex = render->CreateSampler(samp);
	return m_samplerIndex != UINT_MAX;
}

bool Water::Init(DXRender* render, const char* waterproPath, const char* particleTxdPath)
{
	m_vb = nullptr;
	m_ib = nullptr;
	m_oceanIB = nullptr;
	m_rootSig = nullptr;
	m_pso = nullptr;
	m_texture = {};
	m_samplerIndex = UINT_MAX;
	m_coastIndexCount = 0;
	m_coastVertexBytes = 0;
	m_uvU = 0.0f;
	m_uvV = 0.0f;
	m_time = 0.0f;
	m_seaZ = 6.0f;
	m_ready = false;
	m_numLevels = 0;
	memset(m_waterZs, 0, sizeof(m_waterZs));
	memset(m_largeBlocks, NO_WATER, sizeof(m_largeBlocks));
	memset(m_fineBlocks, NO_WATER, sizeof(m_fineBlocks));

	if (!LoadWaterPro(waterproPath))
		return false;

	ComputeSeaLevel();

	if (!LoadWaterTexture(render, particleTxdPath))
		return false;
	if (!CreatePipeline(render))
		return false;
	if (!BuildCoastMesh(render))
		return false;
	if (!CreateOceanBuffers(render))
		return false;

	m_ready = true;
	return true;
}

void Water::Update(float deltaTime)
{
	if (!m_ready)
		return;

	float dt = deltaTime;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.1f) dt = 0.1f;
	m_time += dt;

	float windAddUV = 0.0028f * (dt * 30.0f);
	m_uvU += windAddUV;
	m_uvV += windAddUV * 0.72f;
	if (m_uvU >= 1.0f) m_uvU -= 1.0f;
	if (m_uvV >= 1.0f) m_uvV -= 1.0f;
}

void Water::BindPipeline(DXRender* render, Camera* camera, float drawDistance,
	FXMVECTOR sunDirToward, bool reflectClouds)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	XMVECTOR camPos = camera->GetPosition();

	WaterCB cb;
	XMMATRIX wvp = camera->GetView() * camera->GetProjection();
	cb.wvp = XMMatrixTranspose(wvp);
	cb.uvScroll = XMFLOAT2(m_uvU, m_uvV);
	cb.fogStart = drawDistance * 0.40f;
	cb.fogEnd = drawDistance * 0.82f;
	cb.tint = XMFLOAT4(0.2352f, 0.3472f, 0.4032f, 0.82f);
	cb.fogColor = XMFLOAT4(0.49804f, 0.78431f, 0.94510f, 1.0f);
	cb.cameraPos = XMFLOAT3(XMVectorGetX(camPos), XMVectorGetY(camPos), XMVectorGetZ(camPos));
	cb.time = m_time;
	XMStoreFloat3(&cb.sunDir, XMVector3Normalize(sunDirToward));
	cb.cloudReflect = reflectClouds ? 1.0f : 0.0f;
	cb.windSpeed = 9.0f;
	cb.cloudCoverage = 0.48f;
	cb.cloudDensity = 0.055f;
	cb.pad1 = 0.0f;

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	memcpy(ptr, &cb, sizeof(cb));

	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(m_pso);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(m_texture.srvIndex));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSamplerGpu(m_samplerIndex));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Water::DrawCoast(DXRender* render)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = m_vb->GetGPUVirtualAddress();
	vbv.SizeInBytes = m_coastVertexBytes;
	vbv.StrideInBytes = sizeof(WaterVertex);
	D3D12_INDEX_BUFFER_VIEW ibv = {};
	ibv.BufferLocation = m_ib->GetGPUVirtualAddress();
	ibv.SizeInBytes = m_coastIndexCount * sizeof(uint32_t);
	ibv.Format = DXGI_FORMAT_R32_UINT;
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->IASetIndexBuffer(&ibv);
	cmd->DrawIndexedInstanced(m_coastIndexCount, 1, 0, 0, 0);
}

void Water::DrawInfiniteOcean(DXRender* render, Camera* camera, float drawDistance)
{
	XMVECTOR camPos = camera->GetPosition();
	float gtaX = XMVectorGetX(camPos);
	float gtaY = XMVectorGetZ(camPos);

	const float tileSize = 256.0f;
	const float uvScale = 8.0f;
	const float seaRenderZ = m_seaZ - WATER_Z_OFFSET;
	const float radius = drawDistance + tileSize;

	int i0 = (int)floorf((gtaX - radius) / tileSize);
	int i1 = (int)floorf((gtaX + radius) / tileSize);
	int j0 = (int)floorf((gtaY - radius) / tileSize);
	int j1 = (int)floorf((gtaY + radius) / tileSize);

	std::vector<WaterVertex> verts;
	verts.reserve(512 * 4);

	float radiusSq = radius * radius;

	for (int i = i0; i <= i1; i++) {
		for (int j = j0; j <= j1; j++) {
			float gx = (float)i * tileSize;
			float gy = (float)j * tileSize;
			float cx = gx + tileSize * 0.5f;
			float cy = gy + tileSize * 0.5f;

			float dx = cx - gtaX;
			float dy = cy - gtaY;
			if (dx * dx + dy * dy > radiusSq)
				continue;

			if (IsInsideMapWaterGrid(cx, cy))
				continue;

			if ((int)(verts.size() / 4) >= MAX_OCEAN_QUADS)
				break;

			WaterVertex v0 = { gx,             seaRenderZ, gy,             0.0f,    0.0f };
			WaterVertex v1 = { gx,             seaRenderZ, gy + tileSize,  0.0f,    uvScale };
			WaterVertex v2 = { gx + tileSize,  seaRenderZ, gy + tileSize,  uvScale, uvScale };
			WaterVertex v3 = { gx + tileSize,  seaRenderZ, gy,             uvScale, 0.0f };
			verts.push_back(v0);
			verts.push_back(v1);
			verts.push_back(v2);
			verts.push_back(v3);
		}
	}

	uint32_t quadCount = (uint32_t)(verts.size() / 4);
	if (quadCount == 0)
		return;

	UINT64 bytes = sizeof(WaterVertex) * verts.size();
	D3D12_GPU_VIRTUAL_ADDRESS vbAddr = 0;
	void* mapped = render->AllocFrameConstants(bytes, &vbAddr);
	memcpy(mapped, verts.data(), (size_t)bytes);

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = vbAddr;
	vbv.SizeInBytes = (UINT)bytes;
	vbv.StrideInBytes = sizeof(WaterVertex);
	D3D12_INDEX_BUFFER_VIEW ibv = {};
	ibv.BufferLocation = m_oceanIB->GetGPUVirtualAddress();
	ibv.SizeInBytes = MAX_OCEAN_QUADS * 6 * sizeof(uint32_t);
	ibv.Format = DXGI_FORMAT_R32_UINT;
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->IASetIndexBuffer(&ibv);
	cmd->DrawIndexedInstanced(quadCount * 6, 1, 0, 0, 0);
}

void Water::Render(DXRender* render, Camera* camera, Frustum& frustum, float drawDistance,
	FXMVECTOR sunDirToward, bool reflectClouds)
{
	if (!m_ready)
		return;

	(void)frustum;

	BindPipeline(render, camera, drawDistance, sunDirToward, reflectClouds);
	DrawCoast(render);
	DrawInfiniteOcean(render, camera, drawDistance);

	render->SetOpaqueState();
	render->ApplyRasterizerState();
}

void Water::Cleanup()
{
	if (m_texture.resource) {
		m_texture.resource->Release();
		m_texture.resource = nullptr;
		m_texture.srvIndex = UINT_MAX;
	}
	if (m_pso) { m_pso->Release(); m_pso = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
	if (m_oceanIB) { m_oceanIB->Release(); m_oceanIB = nullptr; }
	if (m_ib) { m_ib->Release(); m_ib = nullptr; }
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	m_ready = false;
}
