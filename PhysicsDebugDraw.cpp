#include "PhysicsDebugDraw.h"

#include <stdio.h>
#include <d3dcompiler.h>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

PhysicsDebugDraw::PhysicsDebugDraw()
	: m_enabled(false)
	, m_debugMode(DBG_NoDebug)
	, m_cullX(0), m_cullY(0), m_cullZ(0), m_cullRadiusSq(0)
	, m_cullEnabled(false)
	, m_vs(nullptr)
	, m_ps(nullptr)
	, m_layout(nullptr)
	, m_vb(nullptr)
	, m_cb(nullptr)
	, m_rs(nullptr)
	, m_dss(nullptr)
	, m_bs(nullptr)
	, m_vbCapacity(0)
{
	m_viewProj = XMMatrixIdentity();
}

PhysicsDebugDraw::~PhysicsDebugDraw()
{
	Cleanup();
}

bool PhysicsDebugDraw::Init(DXRender* render)
{
	if (!render || !render->GetDevice())
		return false;

	ID3D11Device* device = render->GetDevice();
	HRESULT hr;

	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"debug_line_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] PhysicsDebugDraw: cannot read debug_line_vs.cso\n");
		return false;
	}
	hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
	if (FAILED(hr)) {
		vsBlob->Release();
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = device->CreateInputLayout(
		layout, ARRAYSIZE(layout),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		&m_layout);
	vsBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"debug_line_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] PhysicsDebugDraw: cannot read debug_line_ps.cso\n");
		return false;
	}
	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(XMMATRIX);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&cbd, nullptr, &m_cb);
	if (FAILED(hr))
		return false;

	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	rd.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rd, &m_rs);
	if (FAILED(hr))
		return false;

	D3D11_DEPTH_STENCIL_DESC dd = {};
	dd.DepthEnable = TRUE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = device->CreateDepthStencilState(&dd, &m_dss);
	if (FAILED(hr))
		return false;

	D3D11_BLEND_DESC bd = {};
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&bd, &m_bs);
	if (FAILED(hr))
		return false;

	m_lines.reserve(65536);
	printf("[Info] PhysicsDebugDraw ready (toggle F3)\n");
	return true;
}

void PhysicsDebugDraw::Cleanup()
{
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_vb) { m_vb->Release(); m_vb = nullptr; }
	if (m_cb) { m_cb->Release(); m_cb = nullptr; }
	if (m_rs) { m_rs->Release(); m_rs = nullptr; }
	if (m_dss) { m_dss->Release(); m_dss = nullptr; }
	if (m_bs) { m_bs->Release(); m_bs = nullptr; }
	m_vbCapacity = 0;
	m_lines.clear();
}

void PhysicsDebugDraw::SetEnabled(bool enabled)
{
	m_enabled = enabled;
	if (enabled) {
		m_debugMode =
			DBG_DrawWireframe |
			DBG_DrawAabb |
			DBG_DrawContactPoints |
			DBG_DrawConstraints |
			DBG_DrawConstraintLimits;
	} else {
		m_debugMode = DBG_NoDebug;
		m_lines.clear();
	}
}

void PhysicsDebugDraw::BeginFrame()
{
	m_lines.clear();
}

void PhysicsDebugDraw::SetViewProjection(const XMMATRIX& viewProj)
{
	m_viewProj = viewProj;
}

void PhysicsDebugDraw::SetCullSphere(float x, float y, float z, float radius)
{
	m_cullX = x;
	m_cullY = y;
	m_cullZ = z;
	m_cullRadiusSq = radius * radius;
	m_cullEnabled = radius > 0.0f;
}

void PhysicsDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	if (!m_enabled)
		return;

	float fx = (float)from.x();
	float fy = (float)from.y();
	float fz = (float)from.z();
	float tx = (float)to.x();
	float ty = (float)to.y();
	float tz = (float)to.z();

	if (m_cullEnabled) {
		float mx = (fx + tx) * 0.5f - m_cullX;
		float my = (fy + ty) * 0.5f - m_cullY;
		float mz = (fz + tz) * 0.5f - m_cullZ;
		if (mx * mx + my * my + mz * mz > m_cullRadiusSq)
			return;
	}

	/* Cap to keep frame time sane over huge COL worlds. */
	if (m_lines.size() >= 400000)
		return;

	float r = (float)color.x();
	float g = (float)color.y();
	float b = (float)color.z();

	Vertex a = { fx, fy, fz, r, g, b, 1.0f };
	Vertex c = { tx, ty, tz, r, g, b, 1.0f };
	m_lines.push_back(a);
	m_lines.push_back(c);
}

void PhysicsDebugDraw::drawContactPoint(
	const btVector3& PointOnB, const btVector3& normalOnB,
	btScalar distance, int /*lifeTime*/, const btVector3& color)
{
	btVector3 to = PointOnB + normalOnB * distance;
	drawLine(PointOnB, to, color);
}

void PhysicsDebugDraw::reportErrorWarning(const char* warningString)
{
	if (warningString)
		printf("[Bullet] %s\n", warningString);
}

void PhysicsDebugDraw::draw3dText(const btVector3& /*location*/, const char* /*textString*/)
{
}

void PhysicsDebugDraw::setDebugMode(int debugMode)
{
	m_debugMode = debugMode;
	m_enabled = (debugMode != DBG_NoDebug);
}

int PhysicsDebugDraw::getDebugMode() const
{
	return m_debugMode;
}

bool PhysicsDebugDraw::EnsureVertexBuffer(DXRender* render, UINT vertexCount)
{
	if (vertexCount == 0)
		return false;
	if (m_vb && m_vbCapacity >= vertexCount)
		return true;

	if (m_vb) {
		m_vb->Release();
		m_vb = nullptr;
	}

	UINT cap = vertexCount;
	if (cap < 4096)
		cap = 4096;
	/* Grow with headroom. */
	while (cap < vertexCount)
		cap *= 2;

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * cap;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = render->GetDevice()->CreateBuffer(&bd, nullptr, &m_vb);
	if (FAILED(hr)) {
		m_vbCapacity = 0;
		return false;
	}
	m_vbCapacity = cap;
	return true;
}

void PhysicsDebugDraw::Render(DXRender* render)
{
	if (!m_enabled || !render || m_lines.empty() || !m_vs || !m_ps)
		return;

	UINT count = (UINT)m_lines.size();
	if (!EnsureVertexBuffer(render, count))
		return;

	ID3D11DeviceContext* dc = render->GetDeviceContext();

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(dc->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		memcpy(mapped.pData, m_lines.data(), sizeof(Vertex) * count);
		dc->Unmap(m_vb, 0);
	} else {
		return;
	}

	if (SUCCEEDED(dc->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		XMMATRIX* dst = (XMMATRIX*)mapped.pData;
		*dst = XMMatrixTranspose(m_viewProj);
		dc->Unmap(m_cb, 0);
	}

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	dc->IASetInputLayout(m_layout);
	dc->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	dc->VSSetShader(m_vs, nullptr, 0);
	dc->PSSetShader(m_ps, nullptr, 0);
	dc->VSSetConstantBuffers(0, 1, &m_cb);
	dc->RSSetState(m_rs);
	dc->OMSetDepthStencilState(m_dss, 0);
	float blendFactor[4] = { 0, 0, 0, 0 };
	dc->OMSetBlendState(m_bs, blendFactor, 0xffffffff);

	dc->Draw(count, 0);

	/* Restore typical scene states. */
	render->SetOpaqueState();
	render->ApplyRasterizerState();
}
