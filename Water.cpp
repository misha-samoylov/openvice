#include "Water.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include <Dds.h>
#include <DirectXTex.h>

#include "renderware.h"

#define WATER_X_OFFSET 400.0f
#define WATER_Z_OFFSET 0.5f
#define SMALL_SECTOR_SIZE 32.0f
#define LARGE_SECTOR_SIZE 64.0f
#define HUGE_SECTOR_SIZE 128.0f

#define FOURCC_DXT1_W (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT3_W (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4_W (MAKEFOURCC('D','X','T','4'))

struct DDS_File_W {
	DWORD dwMagic;
	DDS_HEADER header;
};

static float WaterFromSmallX(int x)
{
	return (float)(x - 64) * SMALL_SECTOR_SIZE;
}

static float WaterFromSmallY(int y)
{
	return (float)(y - 64) * SMALL_SECTOR_SIZE;
}

static float WaterFromLargeX(int x)
{
	return (float)(x - 32) * LARGE_SECTOR_SIZE;
}

static float WaterFromLargeY(int y)
{
	return (float)(y - 32) * LARGE_SECTOR_SIZE;
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

	/* CRect[48] = 4 floats each — skip after reading into dummy */
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

	DDS_File_W dds;
	ZeroMemory(&dds, sizeof(dds));
	dds.dwMagic = DDS_MAGIC;
	dds.header.size = sizeof(DDS_HEADER);
	dds.header.width = waterTex->width[0];
	dds.header.height = waterTex->height[0];
	dds.header.pitchOrLinearSize = waterTex->width[0] * waterTex->height[0];
	dds.header.ddspf.size = sizeof(DDS_PIXELFORMAT);
	dds.header.ddspf.flags = DDS_FOURCC;

	switch (waterTex->dxtCompression) {
	default:
	case 1:
		dds.header.ddspf.fourCC = FOURCC_DXT1_W;
		break;
	case 3:
		dds.header.ddspf.fourCC = FOURCC_DXT3_W;
		break;
	case 4:
		dds.header.ddspf.fourCC = FOURCC_DXT4_W;
		break;
	}

	size_t texSize = waterTex->dataSizes[0];
	size_t len = sizeof(dds) + texSize;
	uint8_t* ddsBuf = (uint8_t*)malloc(len);
	memcpy(ddsBuf, &dds, sizeof(dds));
	memcpy(ddsBuf + sizeof(dds), waterTex->texels[0], texSize);

	ScratchImage image;
	HRESULT hr = LoadFromDDSMemory(ddsBuf, len, DDS_FLAGS_NONE, nullptr, image);
	free(ddsBuf);

	if (FAILED(hr)) {
		printf("[Error] Failed to parse water DDS\n");
		free(buffer);
		return false;
	}

	hr = CreateShaderResourceView(
		render->GetDevice(),
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		&m_texture
	);

	free(buffer);

	if (FAILED(hr)) {
		printf("[Error] Failed to create water texture SRV\n");
		return false;
	}

	printf("[Info] Loaded water texture waterclear256 %ux%u\n",
		waterTex->width[0], waterTex->height[0]);
	return true;
}

void Water::AddQuad(std::vector<WaterVertex>& verts, std::vector<uint32_t>& indices,
	float gx, float gy, float gz, float size, float uvScale)
{
	/*
	 * GTA RW: X/Y horizontal, Z up.
	 * openvice LH: X = GTA X, Y = GTA Z, Z = GTA Y.
	 */
	uint32_t base = (uint32_t)verts.size();

	WaterVertex v0 = { gx,        gz, gy,        0.0f,     0.0f };
	WaterVertex v1 = { gx,        gz, gy + size, 0.0f,     uvScale };
	WaterVertex v2 = { gx + size, gz, gy + size, uvScale,  uvScale };
	WaterVertex v3 = { gx + size, gz, gy,        uvScale,  0.0f };

	verts.push_back(v0);
	verts.push_back(v1);
	verts.push_back(v2);
	verts.push_back(v3);

	/* Same winding as re3 after axis remap (matches CULL_FRONT map path). */
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

	/* Fine 32x32 sectors — coastal / inland water from waterpro.dat */
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

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = (UINT)(sizeof(WaterVertex) * verts.size());
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vdata;
	ZeroMemory(&vdata, sizeof(vdata));
	vdata.pSysMem = verts.data();

	HRESULT hr = render->GetDevice()->CreateBuffer(&vbd, &vdata, &m_vb);
	if (FAILED(hr)) {
		printf("[Error] Cannot create water VB\n");
		return false;
	}

	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = (UINT)(sizeof(uint32_t) * indices.size());
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA idata;
	ZeroMemory(&idata, sizeof(idata));
	idata.pSysMem = indices.data();

	hr = render->GetDevice()->CreateBuffer(&ibd, &idata, &m_ib);
	if (FAILED(hr)) {
		printf("[Error] Cannot create water IB\n");
		return false;
	}

	m_indexCount = (uint32_t)indices.size();
	printf("[Info] Coast water mesh: %u quads, %u indices\n",
		(unsigned)(verts.size() / 4), m_indexCount);
	return true;
}

bool Water::CreateOceanBuffers(DXRender* render)
{
	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(WaterVertex) * MAX_OCEAN_QUADS * 4;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = render->GetDevice()->CreateBuffer(&vbd, nullptr, &m_oceanVB);
	if (FAILED(hr))
		return false;

	/* Fixed index pattern for up to MAX_OCEAN_QUADS quads */
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

	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = (UINT)(sizeof(uint32_t) * indices.size());
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA idata;
	ZeroMemory(&idata, sizeof(idata));
	idata.pSysMem = indices.data();

	hr = render->GetDevice()->CreateBuffer(&ibd, &idata, &m_oceanIB);
	return SUCCEEDED(hr);
}

void Water::ComputeSeaLevel()
{
	/* Most common water Z from placed fine tiles (VC ocean is ~6.0). */
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
	/* Same index math as re3 WATER_TO_SMALL_SECTOR with WATER_X_OFFSET. */
	float unsignedX = (gtaX + WATER_X_OFFSET) + 2048.0f;
	float unsignedY = gtaY + 2048.0f;
	int fx = (int)floorf(unsignedX / SMALL_SECTOR_SIZE);
	int fy = (int)floorf(unsignedY / SMALL_SECTOR_SIZE);
	return fx >= 0 && fx < SMALL_SECTORS && fy >= 0 && fy < SMALL_SECTORS;
}

bool Water::CreatePipeline(DXRender* render)
{
	HRESULT hr;

	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"water_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] Cannot read water_vs.cso\n");
		return false;
	}

	hr = render->GetDevice()->CreateVertexShader(
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
	if (FAILED(hr)) {
		vsBlob->Release();
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[2] = {};
	layout[0].SemanticName = "POSITION";
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[0].AlignedByteOffset = 0;
	layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	layout[1].SemanticName = "TEXCOORD";
	layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	layout[1].AlignedByteOffset = 12;
	layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	hr = render->GetDevice()->CreateInputLayout(
		layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
	vsBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"water_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Cannot read water_ps.cso\n");
		return false;
	}

	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = sizeof(WaterCB);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = render->GetDevice()->CreateBuffer(&cbd, nullptr, &m_cb);
	if (FAILED(hr))
		return false;

	D3D11_SAMPLER_DESC samp;
	ZeroMemory(&samp, sizeof(samp));
	samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samp.MaxLOD = D3D11_FLOAT32_MAX;
	hr = render->GetDevice()->CreateSamplerState(&samp, &m_sampler);
	if (FAILED(hr))
		return false;

	D3D11_RASTERIZER_DESC rs;
	ZeroMemory(&rs, sizeof(rs));
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_NONE;
	rs.DepthClipEnable = TRUE;
	hr = render->GetDevice()->CreateRasterizerState(&rs, &m_rasterizer);
	if (FAILED(hr))
		return false;

	D3D11_DEPTH_STENCIL_DESC ds;
	ZeroMemory(&ds, sizeof(ds));
	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = render->GetDevice()->CreateDepthStencilState(&ds, &m_depthState);
	if (FAILED(hr))
		return false;

	return true;
}

bool Water::Init(DXRender* render, const char* waterproPath, const char* particleTxdPath)
{
	m_vb = nullptr;
	m_ib = nullptr;
	m_oceanVB = nullptr;
	m_oceanIB = nullptr;
	m_cb = nullptr;
	m_vs = nullptr;
	m_ps = nullptr;
	m_layout = nullptr;
	m_texture = nullptr;
	m_sampler = nullptr;
	m_rasterizer = nullptr;
	m_depthState = nullptr;
	m_indexCount = 0;
	m_uvU = 0.0f;
	m_uvV = 0.0f;
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

	float windAddUV = (0.0006f) * (deltaTime * 30.0f);
	m_uvU += windAddUV;
	m_uvV += windAddUV;
	if (m_uvU >= 1.0f) m_uvU -= 1.0f;
	if (m_uvV >= 1.0f) m_uvV -= 1.0f;
}

void Water::BindPipeline(DXRender* render, Camera* camera, float drawDistance)
{
	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	WaterCB cb;
	XMMATRIX wvp = camera->GetView() * camera->GetProjection();
	cb.wvp = XMMatrixTranspose(wvp);
	cb.uvScroll = XMFLOAT2(m_uvU, m_uvV);
	cb.fogStart = drawDistance * 0.55f;
	cb.fogEnd = drawDistance * 0.98f;
	cb.tint = XMFLOAT4(0.45f, 0.65f, 0.75f, 0.85f);
	cb.fogColor = XMFLOAT4(0.49804f, 0.78431f, 0.94510f, 1.0f);

	ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);

	ctx->RSSetState(m_rasterizer);
	ctx->OMSetDepthStencilState(m_depthState, 0);

	ctx->IASetInputLayout(m_layout);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ctx->VSSetShader(m_vs, nullptr, 0);
	ctx->VSSetConstantBuffers(0, 1, &m_cb);
	ctx->PSSetShader(m_ps, nullptr, 0);
	ctx->PSSetConstantBuffers(0, 1, &m_cb);
	ctx->PSSetShaderResources(0, 1, &m_texture);
	ctx->PSSetSamplers(0, 1, &m_sampler);
}

void Water::DrawCoast(DXRender* render)
{
	ID3D11DeviceContext* ctx = render->GetDeviceContext();
	UINT stride = sizeof(WaterVertex);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
	ctx->IASetIndexBuffer(m_ib, DXGI_FORMAT_R32_UINT, 0);
	ctx->DrawIndexed(m_indexCount, 0, 0);
}

void Water::DrawInfiniteOcean(DXRender* render, Camera* camera, float drawDistance)
{
	/*
	 * Like re3 extrahuge ocean: fill everything outside the waterpro fine
	 * grid with large tiles at the VC sea level, following the camera so
	 * water appears infinite within the view distance.
	 */
	XMVECTOR camPos = camera->GetPosition();
	float gtaX = XMVectorGetX(camPos);
	float gtaY = XMVectorGetZ(camPos); /* engine Z = GTA Y */

	const float tileSize = 256.0f;
	const float uvScale = 8.0f; /* matches re3 ExtraHuge UV scale */
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

			/* Inside map grid: coast mesh / land already handled. */
			if (IsInsideMapWaterGrid(cx, cy))
				continue;

			if ((int)(verts.size() / 4) >= MAX_OCEAN_QUADS)
				break;

			uint32_t base = (uint32_t)verts.size();
			(void)base;

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

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = ctx->Map(m_oceanVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr))
		return;

	memcpy(mapped.pData, verts.data(), sizeof(WaterVertex) * verts.size());
	ctx->Unmap(m_oceanVB, 0);

	UINT stride = sizeof(WaterVertex);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, &m_oceanVB, &stride, &offset);
	ctx->IASetIndexBuffer(m_oceanIB, DXGI_FORMAT_R32_UINT, 0);
	ctx->DrawIndexed(quadCount * 6, 0, 0);
}

void Water::Render(DXRender* render, Camera* camera, Frustum& frustum, float drawDistance)
{
	if (!m_ready)
		return;

	(void)frustum;

	BindPipeline(render, camera, drawDistance);
	DrawCoast(render);
	DrawInfiniteOcean(render, camera, drawDistance);

	ID3D11DeviceContext* ctx = render->GetDeviceContext();
	ctx->OMSetDepthStencilState(nullptr, 0);
	render->ApplyRasterizerState();
}

void Water::Cleanup()
{
	if (m_depthState) { m_depthState->Release(); m_depthState = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
	if (m_texture) { m_texture->Release(); m_texture = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_oceanIB) { m_oceanIB->Release(); m_oceanIB = nullptr; }
	if (m_oceanVB) { m_oceanVB->Release(); m_oceanVB = nullptr; }
	if (m_ib) { m_ib->Release(); m_ib = nullptr; }
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	m_ready = false;
}
