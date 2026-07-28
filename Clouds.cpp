#include "Clouds.h"
#include "graphics/TextureFactory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "renderware.h"

enum { CLOUD_MAX_QUADS = 64 };
enum { CLOUD_TEX_COUNT = 5 };

/* Midday / sunny from DATA/TIMECYC.DAT (WORLD_HOUR ≈ 12). */
static const float kLowR = 120.0f / 255.0f;
static const float kLowG = 100.0f / 255.0f;
static const float kLowB = 100.0f / 255.0f;
static const float kTopR = 1.0f;
static const float kTopG = 1.0f;
static const float kTopB = 1.0f;
static const float kBotR = 180.0f / 255.0f;
static const float kBotG = 1.0f;
static const float kBotB = 1.0f;

/* re3 Clouds.cpp placement tables */
static const float LowCloudsX[12] = {
	1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 0.8f, -0.8f, 0.4f, -0.4f
};
static const float LowCloudsY[12] = {
	0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.4f, 0.4f, -0.8f, -0.8f
};
static const float LowCloudsZ[12] = {
	0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.3f, 0.9f, 0.4f, 1.3f, 1.4f, 1.2f, 1.7f
};

static const float CoorsOffsetX[37] = {
	0.0f, 60.0f, 72.0f, 48.0f, 21.0f, 12.0f,
	9.0f, -3.0f, -8.4f, -18.0f, -15.0f, -36.0f,
	-40.0f, -48.0f, -60.0f, -24.0f, 100.0f, 100.0f,
	100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
	100.0f, 100.0f, -30.0f, -20.0f, 10.0f, 30.0f,
	0.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f
};
static const float CoorsOffsetY[37] = {
	100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
	100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
	100.0f, 100.0f, 100.0f, 100.0f, -30.0f, 10.0f,
	-25.0f, -5.0f, 28.0f, -10.0f, 10.0f, 0.0f,
	15.0f, 40.0f, -100.0f, -100.0f, -100.0f, -100.0f,
	-100.0f, -40.0f, -20.0f, 0.0f, 10.0f, 30.0f, 35.0f
};
static const float CoorsOffsetZ[37] = {
	2.0f, 1.0f, 0.0f, 0.3f, 0.7f, 1.4f,
	1.7f, 0.24f, 0.7f, 1.3f, 1.6f, 1.0f,
	1.2f, 0.3f, 0.7f, 1.4f, 0.0f, 0.1f,
	0.5f, 0.4f, 0.55f, 0.75f, 1.0f, 1.4f,
	1.7f, 2.0f, 2.0f, 2.3f, 1.9f, 2.4f,
	2.0f, 2.0f, 1.5f, 1.2f, 1.7f, 1.5f, 2.1f
};

static const char* kCloudTexNames[CLOUD_TEX_COUNT] = {
	"cloud1", "cloud2", "cloud3", "cloudhilit", "cloudmasked"
};

static float Clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static void PixelToNdc(float sx, float sy, float screenW, float screenH, float* nx, float* ny)
{
	*nx = (sx / screenW) * 2.0f - 1.0f;
	*ny = 1.0f - (sy / screenH) * 2.0f;
}

static ID3D11ShaderResourceView* CreateSrvFromNative(DXRender* render, NativeTexture* tex)
{
	if (!tex || tex->texels.empty() || !tex->texels[0])
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = TextureFactory::CreateSrvFromDxt(
		render,
		tex->texels[0],
		tex->dataSizes[0],
		tex->width[0],
		tex->height[0],
		tex->dxtCompression,
		&srv);
	return SUCCEEDED(hr) ? srv : nullptr;
}

struct CloudVert {
	float x, y, u, v, r, g, b, a;
};

static void EmitDimQuad(CloudVert* out, int* vertCount,
	float sx, float sy, float halfW, float halfH, float rotation,
	float screenW, float screenH,
	float r, float g, float b, float a)
{
	float c = cosf(rotation);
	float s = sinf(rotation);
	float xs[4], ys[4], us[4], vs[4];
	xs[0] = sx - c * halfW - s * halfH; us[0] = 0.0f; vs[0] = 0.0f;
	xs[1] = sx - c * halfW + s * halfH; us[1] = 0.0f; vs[1] = 1.0f;
	xs[2] = sx + c * halfW + s * halfH; us[2] = 1.0f; vs[2] = 1.0f;
	xs[3] = sx + c * halfW - s * halfH; us[3] = 1.0f; vs[3] = 0.0f;
	ys[0] = sy - c * halfH + s * halfW;
	ys[1] = sy + c * halfH + s * halfW;
	ys[2] = sy + c * halfH - s * halfW;
	ys[3] = sy - c * halfH - s * halfW;

	if (xs[0] < 0 && xs[1] < 0 && xs[2] < 0 && xs[3] < 0) return;
	if (ys[0] < 0 && ys[1] < 0 && ys[2] < 0 && ys[3] < 0) return;
	if (xs[0] > screenW && xs[1] > screenW && xs[2] > screenW && xs[3] > screenW) return;
	if (ys[0] > screenH && ys[1] > screenH && ys[2] > screenH && ys[3] > screenH) return;

	int base = *vertCount;
	if (base + 6 > CLOUD_MAX_QUADS * 6)
		return;

	static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++) {
		int k = idx[i];
		float nx, ny;
		PixelToNdc(xs[k], ys[k], screenW, screenH, &nx, &ny);
		CloudVert& v = out[base + i];
		v.x = nx; v.y = ny;
		v.u = us[k]; v.v = vs[k];
		v.r = r; v.g = g; v.b = b; v.a = a;
	}
	*vertCount = base + 6;
}

static void EmitAspect2ColQuad(CloudVert* out, int* vertCount,
	float sx, float sy, float halfW, float halfH, float rotation,
	float screenW, float screenH,
	float r1, float g1, float b1, float r2, float g2, float b2, float a)
{
	float c = cosf(rotation);
	float s = sinf(rotation);
	float xs[4], ys[4], us[4], vs[4], cf[4];
	xs[0] = sx + halfW * (-c - s); us[0] = 0.0f; vs[0] = 0.0f;
	xs[1] = sx + halfW * (-c + s); us[1] = 0.0f; vs[1] = 1.0f;
	xs[2] = sx + halfW * (+c + s); us[2] = 1.0f; vs[2] = 1.0f;
	xs[3] = sx + halfW * (+c - s); us[3] = 1.0f; vs[3] = 0.0f;
	ys[0] = sy + halfH * (-c + s);
	ys[1] = sy + halfH * (+c + s);
	ys[2] = sy + halfH * (+c - s);
	ys[3] = sy + halfH * (-c - s);

	if (xs[0] < 0 && xs[1] < 0 && xs[2] < 0 && xs[3] < 0) return;
	if (ys[0] < 0 && ys[1] < 0 && ys[2] < 0 && ys[3] < 0) return;
	if (xs[0] > screenW && xs[1] > screenW && xs[2] > screenW && xs[3] > screenW) return;
	if (ys[0] > screenH && ys[1] > screenH && ys[2] > screenH && ys[3] > screenH) return;

	const float cx = 0.0f;
	const float cy = -1.0f;
	cf[0] = Clampf((cx * (-c - s) + cy * (-c + s)) * 0.5f + 0.5f, 0.0f, 1.0f);
	cf[1] = Clampf((cx * (-c + s) + cy * (c + s)) * 0.5f + 0.5f, 0.0f, 1.0f);
	cf[2] = Clampf((cx * (c + s) + cy * (c - s)) * 0.5f + 0.5f, 0.0f, 1.0f);
	cf[3] = Clampf((cx * (c - s) + cy * (-c - s)) * 0.5f + 0.5f, 0.0f, 1.0f);

	int base = *vertCount;
	if (base + 6 > CLOUD_MAX_QUADS * 6)
		return;

	static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++) {
		int k = idx[i];
		float f = cf[k];
		float nx, ny;
		PixelToNdc(xs[k], ys[k], screenW, screenH, &nx, &ny);
		CloudVert& v = out[base + i];
		v.x = nx; v.y = ny;
		v.u = us[k]; v.v = vs[k];
		v.r = r1 * f + r2 * (1.0f - f);
		v.g = g1 * f + g2 * (1.0f - f);
		v.b = b1 * f + b2 * (1.0f - f);
		v.a = a;
	}
	*vertCount = base + 6;
}

bool Clouds::LoadTextures(DXRender* render, const char* particleTxdPath)
{
	FILE* f = fopen(particleTxdPath, "rb");
	if (!f) {
		printf("[Error] Clouds: cannot open %s\n", particleTxdPath);
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

	for (int i = 0; i < TEX_COUNT; i++) {
		m_textures[i] = nullptr;
		NativeTexture* found = nullptr;
		for (size_t t = 0; t < txd.texList.size(); t++) {
			if (_stricmp(txd.texList[t].name, kCloudTexNames[i]) == 0) {
				found = &txd.texList[t];
				break;
			}
		}
		if (!found) {
			printf("[Error] Clouds: texture '%s' not in particle.txd\n", kCloudTexNames[i]);
			free(buffer);
			return false;
		}
		m_textures[i] = CreateSrvFromNative(render, found);
		if (!m_textures[i]) {
			printf("[Error] Clouds: failed to create SRV for '%s'\n", kCloudTexNames[i]);
			free(buffer);
			return false;
		}
	}

	free(buffer);
	printf("[Info] Clouds: loaded cloud1/2/3, cloudhilit, cloudmasked\n");
	return true;
}

bool Clouds::CreatePipeline(DXRender* render)
{
	ID3DBlob* vsBlob = nullptr;
	HRESULT hr = D3DReadFileToBlob(L"cloud_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_vs.cso\n");
		return false;
	}

	hr = render->GetDevice()->CreateVertexShader(
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
	if (FAILED(hr)) {
		vsBlob->Release();
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = render->GetDevice()->CreateInputLayout(
		layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
	vsBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"cloud_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_ps.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(CloudVertex) * MAX_QUADS * 6;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = render->GetDevice()->CreateBuffer(&vbd, nullptr, &m_vb);
	return SUCCEEDED(hr);
}

bool Clouds::CreateStates(DXRender* render)
{
	D3D11_SAMPLER_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	HRESULT hr = render->GetDevice()->CreateSamplerState(&sd, &m_sampler);
	if (FAILED(hr))
		return false;

	D3D11_RASTERIZER_DESC rd;
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	hr = render->GetDevice()->CreateRasterizerState(&rd, &m_rasterizer);
	if (FAILED(hr))
		return false;

	D3D11_DEPTH_STENCIL_DESC ds;
	ZeroMemory(&ds, sizeof(ds));
	ds.DepthEnable = FALSE;
	ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
	hr = render->GetDevice()->CreateDepthStencilState(&ds, &m_depthOff);
	if (FAILED(hr))
		return false;

	D3D11_BLEND_DESC additive;
	ZeroMemory(&additive, sizeof(additive));
	additive.RenderTarget[0].BlendEnable = TRUE;
	additive.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	additive.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	additive.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	additive.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	additive.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	additive.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	additive.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = render->GetDevice()->CreateBlendState(&additive, &m_blendAdditive);
	if (FAILED(hr))
		return false;

	D3D11_BLEND_DESC alpha;
	ZeroMemory(&alpha, sizeof(alpha));
	alpha.RenderTarget[0].BlendEnable = TRUE;
	alpha.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	alpha.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	alpha.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	alpha.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	alpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	alpha.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	alpha.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = render->GetDevice()->CreateBlendState(&alpha, &m_blendAlpha);
	return SUCCEEDED(hr);
}

bool Clouds::Init(DXRender* render, const char* particleTxdPath)
{
	m_vb = nullptr;
	m_vs = nullptr;
	m_ps = nullptr;
	m_layout = nullptr;
	m_sampler = nullptr;
	m_rasterizer = nullptr;
	m_depthOff = nullptr;
	m_blendAdditive = nullptr;
	m_blendAlpha = nullptr;
	for (int i = 0; i < TEX_COUNT; i++)
		m_textures[i] = nullptr;
	m_cloudRotation = 0.0f;
	m_individualRotation = 0;
	m_cameraRoll = 0.0f;
	m_wind = 0.25f;
	m_ready = false;

	if (!LoadTextures(render, particleTxdPath))
		return false;
	if (!CreatePipeline(render))
		return false;
	if (!CreateStates(render))
		return false;

	m_ready = true;
	printf("[Info] Clouds ready (re3 miami style)\n");
	return true;
}

void Clouds::Cleanup()
{
	m_ready = false;
	for (int i = 0; i < TEX_COUNT; i++) {
		if (m_textures[i]) {
			m_textures[i]->Release();
			m_textures[i] = nullptr;
		}
	}
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_depthOff) { m_depthOff->Release(); m_depthOff = nullptr; }
	if (m_blendAdditive) { m_blendAdditive->Release(); m_blendAdditive = nullptr; }
	if (m_blendAlpha) { m_blendAlpha->Release(); m_blendAlpha = nullptr; }
}

void Clouds::Update(float dt, Camera* camera)
{
	if (!m_ready || !camera)
		return;

	float timestep = dt * 30.0f;
	if (timestep < 0.0f) timestep = 0.0f;
	if (timestep > 3.0f) timestep = 3.0f;

	XMMATRIX view = camera->GetView();
	float fx = XMVectorGetZ(view.r[0]);
	float fz = XMVectorGetZ(view.r[2]);
	float orientation = atan2f(-fx, fz);

	float s = sinf(orientation - 0.85f);
	m_cloudRotation += m_wind * s * 0.001f * timestep;
	m_individualRotation += (uint32_t)((m_wind * timestep * 0.5f + 0.3f * timestep) * 60.0f);
	m_cameraRoll = 0.0f;
}

bool Clouds::ProjectGtaPoint(Camera* camera, float gtaX, float gtaY, float gtaZ,
	float screenW, float screenH,
	float* outSX, float* outSY, float* outSzx, float* outSzy) const
{
	XMVECTOR world = XMVectorSet(gtaX, gtaZ, gtaY, 1.0f);
	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMVECTOR viewPos = XMVector4Transform(world, view);
	float vz = XMVectorGetZ(viewPos);
	if (vz <= 1.1f)
		return false;

	XMVECTOR clip = XMVector4Transform(world, viewProj);
	float w = XMVectorGetW(clip);
	if (w <= 0.0001f)
		return false;

	float ndcX = XMVectorGetX(clip) / w;
	float ndcY = XMVectorGetY(clip) / w;

	*outSX = (ndcX + 1.0f) * 0.5f * screenW;
	*outSY = (1.0f - ndcY) * 0.5f * screenH;

	float recip = 1.0f / vz;
	*outSzx = recip * screenW;
	*outSzy = recip * screenH;
	return true;
}

void Clouds::FlushBatch(DXRender* render, ID3D11ShaderResourceView* srv,
	ID3D11BlendState* blend, CloudVertex* verts, int vertCount)
{
	if (vertCount <= 0 || !srv)
		return;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr))
		return;
	memcpy(mapped.pData, verts, sizeof(CloudVertex) * (size_t)vertCount);
	ctx->Unmap(m_vb, 0);

	float bf[4] = { 0, 0, 0, 0 };
	ctx->OMSetBlendState(blend, bf, 0xffffffff);
	ctx->OMSetDepthStencilState(m_depthOff, 0);
	ctx->RSSetState(m_rasterizer);

	ctx->IASetInputLayout(m_layout);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(CloudVertex);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
	ctx->VSSetShader(m_vs, nullptr, 0);
	ctx->PSSetShader(m_ps, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &srv);
	ctx->PSSetSamplers(0, 1, &m_sampler);
	ctx->Draw((UINT)vertCount, 0);
}

void Clouds::Render(DXRender* render, Camera* camera)
{
	if (!m_ready || !render || !camera)
		return;

	float screenW = (float)render->GetBackBufferWidth();
	float screenH = (float)render->GetBackBufferHeight();
	if (screenW < 1.0f || screenH < 1.0f)
		return;

	XMVECTOR cam = camera->GetPosition();
	float camGtaX = XMVectorGetX(cam);
	float camGtaY = XMVectorGetZ(cam);

	/* CloudVertex layout matches CloudVert */
	static_assert(sizeof(CloudVertex) == sizeof(CloudVert), "cloud vertex layout");

	CloudVert verts[CLOUD_MAX_QUADS * 6];
	int vertCount = 0;

	for (int cloudtype = 0; cloudtype < 3; cloudtype++) {
		vertCount = 0;
		for (int i = cloudtype; i < 12; i += 3) {
			float gtaX = camGtaX + 800.0f * LowCloudsX[i];
			float gtaY = camGtaY + 800.0f * LowCloudsY[i];
			float gtaZ = 40.0f + 60.0f * LowCloudsZ[i];

			float sx, sy, szx, szy;
			if (!ProjectGtaPoint(camera, gtaX, gtaY, gtaZ, screenW, screenH,
				&sx, &sy, &szx, &szy))
				continue;

			EmitDimQuad(verts, &vertCount,
				sx, sy, szx * 320.0f, szy * 40.0f, m_cameraRoll,
				screenW, screenH,
				kLowR, kLowG, kLowB, 1.0f);
		}
		FlushBatch(render, m_textures[cloudtype], m_blendAdditive,
			reinterpret_cast<CloudVertex*>(verts), vertCount);
	}

	float rotSin = sinf(m_cloudRotation);
	float rotCos = cosf(m_cloudRotation);
	float fluffyAlpha = 160.0f / 255.0f;
	float spin = ((m_individualRotation & 0xFFFFu) / 65336.0f) * 6.28f + m_cameraRoll;

	vertCount = 0;
	for (int i = 0; i < 37; i++) {
		float px = 2.0f * CoorsOffsetX[i];
		float py = 2.0f * CoorsOffsetY[i];
		float pz = 40.0f * CoorsOffsetZ[i] + 40.0f;
		float gtaX = px * rotCos + py * rotSin + camGtaX;
		float gtaY = px * rotSin - py * rotCos + camGtaY;
		float gtaZ = pz;

		float sx, sy, szx, szy;
		if (!ProjectGtaPoint(camera, gtaX, gtaY, gtaZ, screenW, screenH,
			&sx, &sy, &szx, &szy))
			continue;

		EmitAspect2ColQuad(verts, &vertCount,
			sx, sy, szx * 55.0f, szy * 55.0f, spin,
			screenW, screenH,
			kTopR, kTopG, kTopB, kBotR, kBotG, kBotB, fluffyAlpha);
	}
	FlushBatch(render, m_textures[TEX_MASKED], m_blendAlpha,
		reinterpret_cast<CloudVertex*>(verts), vertCount);

	render->SetOpaqueState();
	render->ApplyRasterizerState();
}
