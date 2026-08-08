#include "ui/Sprite2d.h"

#include <stdio.h>
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

Sprite2d::Sprite2d()
	: m_screenW(1)
	, m_screenH(1)
	, m_vs(nullptr)
	, m_ps(nullptr)
	, m_layout(nullptr)
	, m_vb(nullptr)
	, m_psCB(nullptr)
	, m_sampler(nullptr)
	, m_blend(nullptr)
	, m_depthOff(nullptr)
	, m_rs(nullptr)
	, m_whiteSRV(nullptr)
	, m_whiteTex(nullptr)
	, m_render(nullptr)
	, m_currentSrv(nullptr)
	, m_ready(false)
{
}

Sprite2d::~Sprite2d()
{
	Shutdown();
}

bool Sprite2d::Init(DXRender* render)
{
	if (!render)
		return false;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;

	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"hud_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] Sprite2d: cannot read hud_vs.cso\n");
		return false;
	}
	hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
	if (FAILED(hr)) {
		vsBlob->Release();
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = device->CreateInputLayout(layout, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
	vsBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"hud_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Sprite2d: cannot read hud_ps.cso\n");
		return false;
	}
	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(Vertex) * kMaxVerts;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&vbd, nullptr, &m_vb);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.ByteWidth = 32;
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&cbd, nullptr, &m_psCB);
	if (FAILED(hr))
		return false;

	D3D11_SAMPLER_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sd, &m_sampler);
	if (FAILED(hr))
		return false;

	D3D11_BLEND_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_blend);
	if (FAILED(hr))
		return false;

	D3D11_DEPTH_STENCIL_DESC dd;
	ZeroMemory(&dd, sizeof(dd));
	dd.DepthEnable = FALSE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = device->CreateDepthStencilState(&dd, &m_depthOff);
	if (FAILED(hr))
		return false;

	D3D11_RASTERIZER_DESC rd;
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = FALSE;
	hr = device->CreateRasterizerState(&rd, &m_rs);
	if (FAILED(hr))
		return false;

	uint32_t white = 0xFFFFFFFFu;
	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = 1;
	td.Height = 1;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_IMMUTABLE;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA init;
	ZeroMemory(&init, sizeof(init));
	init.pSysMem = &white;
	init.SysMemPitch = 4;
	hr = device->CreateTexture2D(&td, &init, &m_whiteTex);
	if (FAILED(hr))
		return false;
	hr = device->CreateShaderResourceView(m_whiteTex, nullptr, &m_whiteSRV);
	if (FAILED(hr))
		return false;

	m_ready = true;
	printf("[Info] Sprite2d ready\n");
	return true;
}

void Sprite2d::Shutdown()
{
	m_verts.clear();
	m_currentSrv = nullptr;
	m_ready = false;
	if (m_whiteSRV) { m_whiteSRV->Release(); m_whiteSRV = nullptr; }
	if (m_whiteTex) { m_whiteTex->Release(); m_whiteTex = nullptr; }
	if (m_rs) { m_rs->Release(); m_rs = nullptr; }
	if (m_depthOff) { m_depthOff->Release(); m_depthOff = nullptr; }
	if (m_blend) { m_blend->Release(); m_blend = nullptr; }
	if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
	if (m_psCB) { m_psCB->Release(); m_psCB = nullptr; }
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}

void Sprite2d::PixelToNdc(float sx, float sy, float* nx, float* ny) const
{
	*nx = (sx / (float)m_screenW) * 2.0f - 1.0f;
	*ny = 1.0f - (sy / (float)m_screenH) * 2.0f;
}

void Sprite2d::Begin(DXRender* render)
{
	if (!m_ready || !render)
		return;
	m_render = render;
	m_screenW = render->GetBackBufferWidth();
	m_screenH = render->GetBackBufferHeight();
	if (m_screenW == 0) m_screenW = 1;
	if (m_screenH == 0) m_screenH = 1;
	m_verts.clear();
	m_currentSrv = nullptr;

	ID3D11DeviceContext* dc = render->GetDeviceContext();
	render->BindBackBufferOnly();

	dc->IASetInputLayout(m_layout);
	dc->VSSetShader(m_vs, nullptr, 0);
	dc->PSSetShader(m_ps, nullptr, 0);
	dc->PSSetSamplers(0, 1, &m_sampler);
	dc->PSSetConstantBuffers(0, 1, &m_psCB);
	dc->RSSetState(m_rs);
	dc->OMSetDepthStencilState(m_depthOff, 0);
	float blendFactor[4] = { 0, 0, 0, 0 };
	dc->OMSetBlendState(m_blend, blendFactor, 0xffffffff);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(dc->Map(m_psCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		float* f = (float*)mapped.pData;
		f[0] = f[1] = f[2] = f[3] = 1.0f;
		f[4] = f[5] = f[6] = f[7] = 0.0f;
		dc->Unmap(m_psCB, 0);
	}
}

void Sprite2d::FlushCurrent()
{
	if (m_render)
		Flush(m_render);
}

void Sprite2d::DrawRect(
	float left, float top, float right, float bottom,
	float u0, float v0, float u1, float v1,
	float r, float g, float b, float a,
	ID3D11ShaderResourceView* srv,
	bool circleClip)
{
	float nx[4], ny[4];
	PixelToNdc(left, top, &nx[0], &ny[0]);
	PixelToNdc(right, top, &nx[1], &ny[1]);
	PixelToNdc(right, bottom, &nx[2], &ny[2]);
	PixelToNdc(left, bottom, &nx[3], &ny[3]);

	float u[4] = { u0, u1, u1, u0 };
	float v[4] = { v0, v0, v1, v1 };
	float cu[4], cv[4];
	if (circleClip) {
		cu[0] = 0.0f; cv[0] = 0.0f;
		cu[1] = 1.0f; cv[1] = 0.0f;
		cu[2] = 1.0f; cv[2] = 1.0f;
		cu[3] = 0.0f; cv[3] = 1.0f;
	} else {
		for (int i = 0; i < 4; i++) {
			cu[i] = -100.0f;
			cv[i] = -100.0f;
		}
	}

	DrawQuad(nx, ny, u, v, cu, cv, r, g, b, a, srv);
}

void Sprite2d::DrawQuad(
	const float px[4], const float py[4],
	const float u[4], const float v[4],
	const float cu[4], const float cv[4],
	float r, float g, float b, float a,
	ID3D11ShaderResourceView* srv)
{
	if (!m_ready)
		return;
	if (!srv)
		srv = m_whiteSRV;

	if (m_currentSrv && m_currentSrv != srv && !m_verts.empty())
		FlushCurrent();
	if ((int)m_verts.size() + 6 > kMaxVerts)
		FlushCurrent();
	m_currentSrv = srv;

	Vertex verts[4];
	for (int i = 0; i < 4; i++) {
		verts[i].x = px[i];
		verts[i].y = py[i];
		verts[i].u = u[i];
		verts[i].v = v[i];
		verts[i].cu = cu[i];
		verts[i].cv = cv[i];
		verts[i].r = r;
		verts[i].g = g;
		verts[i].b = b;
		verts[i].a = a;
	}

	static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++)
		m_verts.push_back(verts[idx[i]]);
}

void Sprite2d::Flush(DXRender* render)
{
	if (!m_ready || !render || m_verts.empty()) {
		m_verts.clear();
		m_currentSrv = nullptr;
		return;
	}

	ID3D11DeviceContext* dc = render->GetDeviceContext();
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(dc->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		m_verts.clear();
		return;
	}
	memcpy(mapped.pData, m_verts.data(), m_verts.size() * sizeof(Vertex));
	dc->Unmap(m_vb, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	dc->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
	ID3D11ShaderResourceView* srv = m_currentSrv ? m_currentSrv : m_whiteSRV;
	dc->PSSetShaderResources(0, 1, &srv);
	dc->Draw((UINT)m_verts.size(), 0);

	m_verts.clear();
	m_currentSrv = nullptr;
}
