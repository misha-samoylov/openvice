#include "Clouds.h"
#include "graphics/TextureFactory.h"
#include "core/GameConfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "renderware.h"

enum { CLOUD_MAX_QUADS = 8 };

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

bool Clouds::LoadSunTexture(DXRender* render, const char* particleTxdPath)
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

	NativeTexture* found = nullptr;
	for (size_t t = 0; t < txd.texList.size(); t++) {
		if (_stricmp(txd.texList[t].name, "coronastar") == 0) {
			found = &txd.texList[t];
			break;
		}
	}
	if (!found) {
		printf("[Error] Clouds: texture 'coronastar' not in particle.txd\n");
		free(buffer);
		return false;
	}

	m_sunTex = CreateSrvFromNative(render, found);
	free(buffer);
	if (!m_sunTex) {
		printf("[Error] Clouds: failed to create SRV for 'coronastar'\n");
		return false;
	}

	printf("[Info] Clouds: loaded coronastar (volumetric clouds are procedural)\n");
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
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vsSun);
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
	hr = D3DReadFileToBlob(L"cloud_sun_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_sun_ps.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_psSun);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	hr = D3DReadFileToBlob(L"ssao_vs.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read ssao_vs.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreateVertexShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_vsCloud);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	hr = D3DReadFileToBlob(L"cloud_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_ps.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_psCloud);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	hr = D3DReadFileToBlob(L"cloud_composite_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_composite_ps.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_psComposite);
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
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.ByteWidth = sizeof(CloudsCB);
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = render->GetDevice()->CreateBuffer(&cbd, nullptr, &m_cb);
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
	if (FAILED(hr))
		return false;

	D3D11_BLEND_DESC opaque;
	ZeroMemory(&opaque, sizeof(opaque));
	opaque.RenderTarget[0].BlendEnable = FALSE;
	opaque.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = render->GetDevice()->CreateBlendState(&opaque, &m_blendOpaque);
	return SUCCEEDED(hr);
}

void Clouds::ReleaseTargets()
{
	if (m_cloudSRV) { m_cloudSRV->Release(); m_cloudSRV = nullptr; }
	if (m_cloudRTV) { m_cloudRTV->Release(); m_cloudRTV = nullptr; }
	if (m_cloudTex) { m_cloudTex->Release(); m_cloudTex = nullptr; }
	m_fullW = m_fullH = m_halfW = m_halfH = 0;
}

bool Clouds::CreateTargets(DXRender* render)
{
	ReleaseTargets();

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = m_halfW;
	td.Height = m_halfH;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = render->GetDevice()->CreateTexture2D(&td, nullptr, &m_cloudTex);
	if (FAILED(hr))
		return false;
	hr = render->GetDevice()->CreateRenderTargetView(m_cloudTex, nullptr, &m_cloudRTV);
	if (FAILED(hr))
		return false;
	hr = render->GetDevice()->CreateShaderResourceView(m_cloudTex, nullptr, &m_cloudSRV);
	return SUCCEEDED(hr);
}

bool Clouds::Init(DXRender* render, const char* particleTxdPath)
{
	m_vb = nullptr;
	m_vsSun = nullptr;
	m_psSun = nullptr;
	m_layout = nullptr;
	m_vsCloud = nullptr;
	m_psCloud = nullptr;
	m_psComposite = nullptr;
	m_cb = nullptr;
	m_sunTex = nullptr;
	m_sampler = nullptr;
	m_rasterizer = nullptr;
	m_depthOff = nullptr;
	m_blendAdditive = nullptr;
	m_blendAlpha = nullptr;
	m_blendOpaque = nullptr;
	m_cloudTex = nullptr;
	m_cloudRTV = nullptr;
	m_cloudSRV = nullptr;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;
	m_time = 0.0f;
	m_wind = 0.25f;
	m_ready = false;

	if (!LoadSunTexture(render, particleTxdPath))
		return false;
	if (!CreatePipeline(render))
		return false;
	if (!CreateStates(render))
		return false;
	if (!CreateTargets(render))
		return false;

	m_ready = true;
	printf("[Info] Clouds ready (volumetric half-res)\n");
	return true;
}

void Clouds::Cleanup()
{
	m_ready = false;
	ReleaseTargets();
	if (m_sunTex) { m_sunTex->Release(); m_sunTex = nullptr; }
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	if (m_vsSun) { m_vsSun->Release(); m_vsSun = nullptr; }
	if (m_psSun) { m_psSun->Release(); m_psSun = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_vsCloud) { m_vsCloud->Release(); m_vsCloud = nullptr; }
	if (m_psCloud) { m_psCloud->Release(); m_psCloud = nullptr; }
	if (m_psComposite) { m_psComposite->Release(); m_psComposite = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
	if (m_rasterizer) { m_rasterizer->Release(); m_rasterizer = nullptr; }
	if (m_depthOff) { m_depthOff->Release(); m_depthOff = nullptr; }
	if (m_blendAdditive) { m_blendAdditive->Release(); m_blendAdditive = nullptr; }
	if (m_blendAlpha) { m_blendAlpha->Release(); m_blendAlpha = nullptr; }
	if (m_blendOpaque) { m_blendOpaque->Release(); m_blendOpaque = nullptr; }
}

void Clouds::Update(float dt, Camera* camera)
{
	(void)camera;
	if (!m_ready)
		return;

	float timestep = dt;
	if (timestep < 0.0f) timestep = 0.0f;
	if (timestep > 0.1f) timestep = 0.1f;
	m_time += timestep;
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
	ctx->VSSetShader(m_vsSun, nullptr, 0);
	ctx->PSSetShader(m_psSun, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &srv);
	ctx->PSSetSamplers(0, 1, &m_sampler);
	ctx->Draw((UINT)vertCount, 0);
}

void Clouds::DrawFullscreen(ID3D11DeviceContext* ctx)
{
	ctx->IASetInputLayout(nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = 0, offset = 0;
	ID3D11Buffer* nullVB = nullptr;
	ctx->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);
	ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	ctx->VSSetShader(m_vsCloud, nullptr, 0);
	ctx->Draw(3, 0);
}

void Clouds::RenderSun(DXRender* render, Camera* camera, FXMVECTOR sunDirToward,
	float screenW, float screenH)
{
	if (!m_sunTex)
		return;

	if (XMVectorGetY(sunDirToward) < -0.2f)
		return;

	XMVECTOR cam = camera->GetPosition();
	XMVECTOR sunWorld = XMVectorAdd(cam, XMVectorScale(sunDirToward, 150.0f));
	float engX = XMVectorGetX(sunWorld);
	float engY = XMVectorGetY(sunWorld);
	float engZ = XMVectorGetZ(sunWorld);
	float gtaX = engX;
	float gtaY = engZ;
	float gtaZ = engY;

	float sx, sy, szx, szy;
	if (!ProjectGtaPoint(camera, gtaX, gtaY, gtaZ, screenW, screenH,
		&sx, &sy, &szx, &szy))
		return;

	CloudVert verts[12];
	int vertCount = 0;

	float coreSize = 10.0f * SUN_SIZE;
	EmitDimQuad(verts, &vertCount,
		sx, sy, szx * coreSize, szy * coreSize, 0.0f,
		screenW, screenH,
		SUN_CORE_R, SUN_CORE_G, SUN_CORE_B, 1.0f);

	if (XMVectorGetY(sunDirToward) > 0.0f) {
		float coronaSize = 25.0f * SUN_SIZE;
		EmitDimQuad(verts, &vertCount,
			sx, sy, szx * coronaSize, szy * coronaSize, 0.0f,
			screenW, screenH,
			SUN_CORONA_R, SUN_CORONA_G, SUN_CORONA_B, 1.0f);
	}

	FlushBatch(render, m_sunTex, m_blendAdditive,
		reinterpret_cast<CloudVertex*>(verts), vertCount);
}

void Clouds::RenderVolumetric(DXRender* render, Camera* camera, FXMVECTOR sunDirToward)
{
	UINT w = render->GetBackBufferWidth();
	UINT h = render->GetBackBufferHeight();
	if (w != m_fullW || h != m_fullH) {
		if (!CreateTargets(render))
			return;
	}
	if (!m_cloudRTV || !m_cloudSRV)
		return;

	ID3D11DeviceContext* ctx = render->GetDeviceContext();

	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	CloudsCB cb;
	XMStoreFloat4x4(&cb.InvViewProj, XMMatrixTranspose(invViewProj));

	XMVECTOR cam = camera->GetPosition();
	cb.CamPos = XMFLOAT3(XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam));
	cb.Time = m_time;

	XMFLOAT3 sun;
	XMStoreFloat3(&sun, XMVector3Normalize(sunDirToward));
	cb.SunDir = sun;
	cb.Coverage = CLOUD_COVERAGE;

	cb.SkyColor = XMFLOAT3(SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B);
	cb.DensityMult = CLOUD_DENSITY;
	cb.CloudSilver = XMFLOAT3(1.15f, 1.08f, 0.98f);
	cb.Absorption = CLOUD_ABSORPTION;
	cb.CloudBottom = CLOUD_BOTTOM;
	cb.CloudTop = CLOUD_TOP;
	cb.WindSpeed = CLOUD_WIND * (0.5f + m_wind);
	cb.Ambient = CLOUD_AMBIENT;

	ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);

	float bf[4] = { 0, 0, 0, 0 };
	ctx->OMSetDepthStencilState(m_depthOff, 0);
	ctx->RSSetState(m_rasterizer);
	ctx->VSSetConstantBuffers(0, 1, &m_cb);
	ctx->PSSetConstantBuffers(0, 1, &m_cb);

	/* ---- Half-res raymarch ---- */
	D3D11_VIEWPORT halfVP;
	halfVP.TopLeftX = 0.0f;
	halfVP.TopLeftY = 0.0f;
	halfVP.Width = (float)m_halfW;
	halfVP.Height = (float)m_halfH;
	halfVP.MinDepth = 0.0f;
	halfVP.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &halfVP);

	float clearCloud[4] = { 0, 0, 0, 0 };
	ctx->OMSetRenderTargets(1, &m_cloudRTV, nullptr);
	ctx->ClearRenderTargetView(m_cloudRTV, clearCloud);
	ctx->OMSetBlendState(m_blendOpaque, bf, 0xffffffff);
	ctx->PSSetShader(m_psCloud, nullptr, 0);
	DrawFullscreen(ctx);

	/* ---- Upsample + alpha blend onto scene ---- */
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ctx->PSSetShaderResources(0, 1, &nullSRV);
	render->RestoreMainTargets();
	ctx->OMSetDepthStencilState(m_depthOff, 0);
	ctx->RSSetState(m_rasterizer);
	ctx->OMSetBlendState(m_blendAlpha, bf, 0xffffffff);
	ctx->PSSetShader(m_psComposite, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &m_cloudSRV);
	ctx->PSSetSamplers(0, 1, &m_sampler);
	DrawFullscreen(ctx);

	ctx->PSSetShaderResources(0, 1, &nullSRV);
}

void Clouds::Render(DXRender* render, Camera* camera, FXMVECTOR sunDirToward, bool drawClouds)
{
	if (!m_ready || !render || !camera)
		return;

	float screenW = (float)render->GetBackBufferWidth();
	float screenH = (float)render->GetBackBufferHeight();
	if (screenW < 1.0f || screenH < 1.0f)
		return;

	static_assert(sizeof(CloudVertex) == sizeof(CloudVert), "cloud vertex layout");

	RenderSun(render, camera, sunDirToward, screenW, screenH);

	if (drawClouds)
		RenderVolumetric(render, camera, sunDirToward);

	render->SetOpaqueState();
	render->ApplyRasterizerState();
}
