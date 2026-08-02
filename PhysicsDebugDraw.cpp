#include "PhysicsDebugDraw.h"

#include <stdio.h>
#include <d3dcompiler.h>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

PhysicsDebugDraw::PhysicsDebugDraw()
	: m_enabled(false)
	, m_debugMode(DBG_NoDebug)
	, m_overlayMode(0)
	, m_cullX(0), m_cullY(0), m_cullZ(0), m_cullRadiusSq(0)
	, m_cullEnabled(false)
	, m_rootSig(nullptr)
	, m_pso(nullptr)
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

	ID3D12Device* device = render->GetDevice();
	HRESULT hr;

	D3D12_ROOT_PARAMETER param = {};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.Descriptor.ShaderRegister = 0;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 1;
	rsDesc.pParameters = &param;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr))
		return false;
	hr = device->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"debug_line_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] PhysicsDebugDraw: cannot read debug_line_vs.cso\n");
		return false;
	}
	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"debug_line_ps.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] PhysicsDebugDraw: cannot read debug_line_ps.cso\n");
		vsBlob->Release();
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = m_rootSig;
	pso.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	pso.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	pso.RasterizerState.AntialiasedLineEnable = TRUE;
	pso.DepthStencilState.DepthEnable = TRUE;
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso.InputLayout = { layout, 2 };
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso.SampleDesc.Count = render->GetMSAASampleCount();

	hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
	vsBlob->Release();
	psBlob->Release();
	if (FAILED(hr))
		return false;

	m_lines.reserve(65536);
	printf("[Info] PhysicsDebugDraw ready (toggle F3)\n");
	return true;
}

void PhysicsDebugDraw::Cleanup()
{
	if (m_pso) { m_pso->Release(); m_pso = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
	m_lines.clear();
}

void PhysicsDebugDraw::SetEnabled(bool enabled)
{
	SetOverlayMode(enabled ? 1 : 0);
}

void PhysicsDebugDraw::SetOverlayMode(int mode)
{
	if (mode < 0)
		mode = 0;
	if (mode > 3)
		mode = 3;
	m_overlayMode = mode;
	m_enabled = (mode != 0);
	if (!m_enabled) {
		m_debugMode = DBG_NoDebug;
		m_lines.clear();
		return;
	}

	if (mode == 1) {
		m_debugMode =
			DBG_DrawWireframe |
			DBG_DrawContactPoints |
			DBG_DrawConstraints |
			DBG_DrawConstraintLimits;
	} else {
		m_debugMode = DBG_DrawWireframe;
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

void PhysicsDebugDraw::Render(DXRender* render)
{
	if (!m_enabled || !render || m_lines.empty() || !m_rootSig || !m_pso)
		return;

	UINT count = (UINT)m_lines.size();
	UINT64 bytes = sizeof(Vertex) * (UINT64)count;
	D3D12_GPU_VIRTUAL_ADDRESS vbAddr = 0;
	void* mapped = render->AllocFrameConstants(bytes, &vbAddr);
	if (!mapped)
		return;
	memcpy(mapped, m_lines.data(), (size_t)bytes);

	XMMATRIX vp = XMMatrixTranspose(m_viewProj);
	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* cbPtr = render->AllocFrameConstants(sizeof(vp), &cbAddr);
	if (!cbPtr)
		return;
	memcpy(cbPtr, &vp, sizeof(vp));

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = vbAddr;
	vbv.SizeInBytes = (UINT)bytes;
	vbv.StrideInBytes = sizeof(Vertex);

	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(m_pso);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->DrawInstanced(count, 1, 0, 0);

	render->SetOpaqueState();
	render->ApplyRasterizerState();
}
