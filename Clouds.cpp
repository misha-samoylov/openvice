#include "Clouds.h"
#include "core/GameConfig.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <d3dcompiler.h>

enum { CLOUD_MAX_QUADS = 8 };

static void PixelToNdc(float sx, float sy, float screenW, float screenH, float* nx, float* ny)
{
	*nx = (sx / screenW) * 2.0f - 1.0f;
	*ny = 1.0f - (sy / screenH) * 2.0f;
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

bool Clouds::CreatePipeline(DXRender* render)
{
	ID3D12Device* device = render->GetDevice();
	HRESULT hr;

	/* ---- Sun: VB + no CB ---- */
	{
		D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ID3DBlob* sigBlob = nullptr;
		hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
		if (FAILED(hr))
			return false;
		hr = device->CreateRootSignature(
			0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSigSun));
		sigBlob->Release();
		if (FAILED(hr))
			return false;
	}

	ID3DBlob* vsSun = nullptr;
	hr = D3DReadFileToBlob(L"cloud_vs.cso", &vsSun);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_vs.cso\n");
		return false;
	}
	ID3DBlob* psSun = nullptr;
	hr = D3DReadFileToBlob(L"cloud_sun_ps.cso", &psSun);
	if (FAILED(hr)) {
		printf("[Error] Clouds: cannot read cloud_sun_ps.cso\n");
		vsSun->Release();
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC sunPso = {};
	sunPso.pRootSignature = m_rootSigSun;
	sunPso.VS = { vsSun->GetBufferPointer(), vsSun->GetBufferSize() };
	sunPso.PS = { psSun->GetBufferPointer(), psSun->GetBufferSize() };
	sunPso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	sunPso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	sunPso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	sunPso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	sunPso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	sunPso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	sunPso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	sunPso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	sunPso.SampleMask = UINT_MAX;
	sunPso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	sunPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	sunPso.RasterizerState.DepthClipEnable = TRUE;
	sunPso.DepthStencilState.DepthEnable = FALSE;
	sunPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	sunPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sunPso.InputLayout = { layout, 3 };
	sunPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	sunPso.NumRenderTargets = 1;
	sunPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	sunPso.SampleDesc.Count = render->GetMSAASampleCount();
	hr = device->CreateGraphicsPipelineState(&sunPso, IID_PPV_ARGS(&m_psoSun));
	vsSun->Release();
	psSun->Release();
	if (FAILED(hr))
		return false;

	/* ---- Volumetric / composite: CBV + optional SRV/sampler ---- */
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

	D3D12_ROOT_SIGNATURE_DESC cloudRs = {};
	cloudRs.NumParameters = 3;
	cloudRs.pParameters = params;
	ID3DBlob* cloudSig = nullptr;
	hr = D3D12SerializeRootSignature(&cloudRs, D3D_ROOT_SIGNATURE_VERSION_1, &cloudSig, nullptr);
	if (FAILED(hr))
		return false;
	hr = device->CreateRootSignature(
		0, cloudSig->GetBufferPointer(), cloudSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSigCloud));
	cloudSig->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* vsCloud = nullptr;
	ID3DBlob* psCloud = nullptr;
	ID3DBlob* psComp = nullptr;
	hr = D3DReadFileToBlob(L"ssao_vs.cso", &vsCloud);
	if (FAILED(hr)) { printf("[Error] Clouds: cannot read ssao_vs.cso\n"); return false; }
	hr = D3DReadFileToBlob(L"cloud_ps.cso", &psCloud);
	if (FAILED(hr)) { printf("[Error] Clouds: cannot read cloud_ps.cso\n"); return false; }
	hr = D3DReadFileToBlob(L"cloud_composite_ps.cso", &psComp);
	if (FAILED(hr)) { printf("[Error] Clouds: cannot read cloud_composite_ps.cso\n"); return false; }

	D3D12_GRAPHICS_PIPELINE_STATE_DESC cloudPso = {};
	cloudPso.pRootSignature = m_rootSigCloud;
	cloudPso.VS = { vsCloud->GetBufferPointer(), vsCloud->GetBufferSize() };
	cloudPso.PS = { psCloud->GetBufferPointer(), psCloud->GetBufferSize() };
	cloudPso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	cloudPso.SampleMask = UINT_MAX;
	cloudPso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	cloudPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	cloudPso.RasterizerState.DepthClipEnable = TRUE;
	cloudPso.DepthStencilState.DepthEnable = FALSE;
	cloudPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	cloudPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	cloudPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	cloudPso.NumRenderTargets = 1;
	cloudPso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	cloudPso.SampleDesc.Count = 1; /* offscreen cloud buffer is single-sample */
	hr = device->CreateGraphicsPipelineState(&cloudPso, IID_PPV_ARGS(&m_psoCloud));
	if (FAILED(hr)) {
		vsCloud->Release(); psCloud->Release(); psComp->Release();
		return false;
	}

	cloudPso.SampleDesc.Count = render->GetMSAASampleCount();
	cloudPso.PS = { psComp->GetBufferPointer(), psComp->GetBufferSize() };
	cloudPso.BlendState.RenderTarget[0].BlendEnable = TRUE;
	cloudPso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	cloudPso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	cloudPso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	cloudPso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	cloudPso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	cloudPso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	hr = device->CreateGraphicsPipelineState(&cloudPso, IID_PPV_ARGS(&m_psoComposite));
	vsCloud->Release();
	psCloud->Release();
	psComp->Release();
	if (FAILED(hr))
		return false;

	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_samplerIndex = render->CreateSampler(sd);
	return m_samplerIndex != UINT_MAX;
}

void Clouds::ReleaseTargets(DXRender* render)
{
	if (m_cloudTex) {
		if (render)
			render->DeferRelease(m_cloudTex);
		else
			m_cloudTex->Release();
		m_cloudTex = nullptr;
	}
	m_cloudRtv = m_cloudSrv = UINT_MAX;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;
}

bool Clouds::CreateTargets(DXRender* render)
{
	ReleaseTargets(render);

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	HRESULT hr = render->CreateTexture2D(
		m_halfW, m_halfH, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_cloudTex);
	if (FAILED(hr))
		return false;
	m_cloudState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_cloudRtv = render->AllocRtvIndex();
	if (m_cloudRtv == UINT_MAX)
		return false;
	render->GetDevice()->CreateRenderTargetView(m_cloudTex, nullptr, render->GetRtvCpu(m_cloudRtv));
	m_cloudSrv = render->CreateTextureSrv(m_cloudTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	return m_cloudSrv != UINT_MAX;
}

bool Clouds::Init(DXRender* render, const char* particleTxdPath)
{
	(void)particleTxdPath;

	m_rootSigSun = nullptr;
	m_rootSigCloud = nullptr;
	m_psoSun = nullptr;
	m_psoCloud = nullptr;
	m_psoComposite = nullptr;
	m_samplerIndex = UINT_MAX;
	m_cloudTex = nullptr;
	m_cloudRtv = m_cloudSrv = UINT_MAX;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;
	m_time = 0.0f;
	m_wind = 0.25f;
	m_ready = false;

	if (!CreatePipeline(render))
		return false;
	if (!CreateTargets(render))
		return false;

	m_ready = true;
	printf("[Info] Clouds ready (volumetric half-res, procedural sun)\n");
	return true;
}

void Clouds::Cleanup()
{
	m_ready = false;
	ReleaseTargets(nullptr);
	if (m_psoComposite) { m_psoComposite->Release(); m_psoComposite = nullptr; }
	if (m_psoCloud) { m_psoCloud->Release(); m_psoCloud = nullptr; }
	if (m_psoSun) { m_psoSun->Release(); m_psoSun = nullptr; }
	if (m_rootSigCloud) { m_rootSigCloud->Release(); m_rootSigCloud = nullptr; }
	if (m_rootSigSun) { m_rootSigSun->Release(); m_rootSigSun = nullptr; }
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

void Clouds::FlushBatch(DXRender* render, ID3D12PipelineState* pso,
	CloudVertex* verts, int vertCount)
{
	if (vertCount <= 0)
		return;

	UINT64 bytes = sizeof(CloudVertex) * (UINT64)vertCount;
	D3D12_GPU_VIRTUAL_ADDRESS vbAddr = 0;
	void* mapped = render->AllocFrameConstants(bytes, &vbAddr);
	if (!mapped)
		return;
	memcpy(mapped, verts, (size_t)bytes);

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = vbAddr;
	vbv.SizeInBytes = (UINT)bytes;
	vbv.StrideInBytes = sizeof(CloudVertex);

	cmd->SetGraphicsRootSignature(m_rootSigSun);
	cmd->SetPipelineState(pso);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->DrawInstanced((UINT)vertCount, 1, 0, 0);
}

void Clouds::DrawFullscreen(DXRender* render, ID3D12PipelineState* pso)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetPipelineState(pso);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void Clouds::RenderSun(DXRender* render, Camera* camera, FXMVECTOR sunDirToward,
	float screenW, float screenH)
{
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

	float unit = 0.5f * (szx + szy);
	float glowHalf = unit * 36.0f * SUN_SIZE;

	float elev = XMVectorGetY(sunDirToward);
	float high = elev > 0.0f ? elev : 0.0f;
	if (high > 1.0f) high = 1.0f;
	float warmth = 1.0f - high * 0.35f;
	float tintR = 1.0f;
	float tintG = 1.0f - warmth * 0.18f;
	float tintB = 1.0f - warmth * 0.42f;
	float intensity = 0.72f + high * 0.35f;

	CloudVert verts[6];
	int vertCount = 0;
	EmitDimQuad(verts, &vertCount,
		sx, sy, glowHalf, glowHalf, 0.0f,
		screenW, screenH,
		tintR * intensity, tintG * intensity, tintB * intensity, 1.0f);

	FlushBatch(render, m_psoSun, reinterpret_cast<CloudVertex*>(verts), vertCount);
}

void Clouds::RenderVolumetric(DXRender* render, Camera* camera, FXMVECTOR sunDirToward)
{
	UINT w = render->GetBackBufferWidth();
	UINT h = render->GetBackBufferHeight();
	if (w != m_fullW || h != m_fullH) {
		if (!CreateTargets(render))
			return;
	}
	if (m_cloudRtv == UINT_MAX || m_cloudSrv == UINT_MAX)
		return;

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();

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

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

	if (m_cloudState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_cloudTex, m_cloudState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_cloudState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_VIEWPORT halfVP = {};
	halfVP.Width = (float)m_halfW;
	halfVP.Height = (float)m_halfH;
	halfVP.MaxDepth = 1.0f;
	D3D12_RECT halfSc = { 0, 0, (LONG)m_halfW, (LONG)m_halfH };
	D3D12_CPU_DESCRIPTOR_HANDLE cloudRtv = render->GetRtvCpu(m_cloudRtv);
	float clearCloud[4] = { 0, 0, 0, 0 };
	cmd->OMSetRenderTargets(1, &cloudRtv, FALSE, nullptr);
	cmd->ClearRenderTargetView(cloudRtv, clearCloud, 0, nullptr);
	cmd->RSSetViewports(1, &halfVP);
	cmd->RSSetScissorRects(1, &halfSc);
	cmd->SetGraphicsRootSignature(m_rootSigCloud);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoCloud);

	render->Transition(m_cloudTex, m_cloudState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_cloudState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	render->RestoreMainTargets();
	cmd->SetGraphicsRootSignature(m_rootSigCloud);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(m_cloudSrv));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSamplerGpu(m_samplerIndex));
	DrawFullscreen(render, m_psoComposite);
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
