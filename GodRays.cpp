#include "GodRays.h"

#include <stdio.h>
#include <cmath>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct GodRaysCB
{
	XMFLOAT2 LightPosUV;
	float Exposure;
	float Decay;
	float Density;
	float Weight;
	float Threshold;
	float Intensity;
	float DepthCutoff;
	float SunOcclusion;
	XMFLOAT2 Pad;
};

static HRESULT CreateGRPso(
	ID3D12Device* device, ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps, bool additive, DXGI_FORMAT rtvFmt,
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
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	if (additive) {
		pso.BlendState.RenderTarget[0].BlendEnable = TRUE;
		pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
		pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	}
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = rtvFmt;
	pso.SampleDesc.Count = 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

HRESULT GodRays::CreateTargets(DXRender* render)
{
	ReleaseTargets(render);

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

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
		m_halfW, m_halfH, DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_raysTex);
	if (FAILED(hr))
		return hr;
	m_raysState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_raysRtv = render->AllocRtvIndex();
	if (m_raysRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_raysTex, nullptr, render->GetRtvCpu(m_raysRtv));
	m_raysSrv = render->CreateTextureSrv(m_raysTex, DXGI_FORMAT_R16G16B16A16_FLOAT);
	return (m_raysSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void GodRays::ReleaseTargets(DXRender* render)
{
	if (m_raysTex) {
		if (render)
			render->DeferRelease(m_raysTex);
		else
			m_raysTex->Release();
		m_raysTex = nullptr;
	}
	if (m_colorTex) {
		if (render)
			render->DeferRelease(m_colorTex);
		else
			m_colorTex->Release();
		m_colorTex = nullptr;
	}
	m_colorSrv = m_raysRtv = m_raysSrv = UINT_MAX;
}

HRESULT GodRays::Init(DXRender* render)
{
	m_rootSig = nullptr;
	m_psoRays = nullptr;
	m_psoComposite = nullptr;
	m_pointSampler = m_linearSampler = UINT_MAX;
	m_colorTex = nullptr;
	m_raysTex = nullptr;
	m_colorSrv = m_raysRtv = m_raysSrv = UINT_MAX;
	m_fullW = m_fullH = m_halfW = m_halfH = 0;

	ID3D12Device* device = render->GetDevice();
	HRESULT hr;

	D3D12_DESCRIPTOR_RANGE srv0 = {};
	srv0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv0.NumDescriptors = 1;
	srv0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE srv1 = {};
	srv1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv1.NumDescriptors = 1;
	srv1.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE sampRange = {};
	sampRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	sampRange.NumDescriptors = 2;
	sampRange.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER params[4] = {};
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
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &sampRange;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 4;
	rsDesc.pParameters = params;

	ID3DBlob* sigBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	ID3DBlob* vs = nullptr;
	ID3DBlob* psRays = nullptr;
	ID3DBlob* psComp = nullptr;
	hr = D3DReadFileToBlob(L"ssao_vs.cso", &vs);
	if (FAILED(hr)) { printf("Error: GodRays cannot read ssao_vs.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"godrays_ps.cso", &psRays);
	if (FAILED(hr)) { printf("Error: GodRays cannot read godrays_ps.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"godrays_composite_ps.cso", &psComp);
	if (FAILED(hr)) { printf("Error: GodRays cannot read godrays_composite_ps.cso\n"); return hr; }

	hr = CreateGRPso(device, m_rootSig, vs, psRays, false, DXGI_FORMAT_R16G16B16A16_FLOAT, &m_psoRays);
	if (FAILED(hr)) return hr;
	hr = CreateGRPso(device, m_rootSig, vs, psComp, true, DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoComposite);
	vs->Release();
	psRays->Release();
	psComp->Release();
	if (FAILED(hr))
		return hr;

	/* Contiguous linear (s0) + point (s1) for rays pass. */
	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_linearSampler = render->CreateSampler(sd);
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	m_pointSampler = render->CreateSampler(sd);
	if (m_linearSampler == UINT_MAX || m_pointSampler == UINT_MAX)
		return E_FAIL;
	/* Rays pass expects s0=linear, s1=point as a contiguous table. */
	if (m_pointSampler != m_linearSampler + 1) {
		printf("Error: GodRays samplers not contiguous\n");
		return E_FAIL;
	}

	hr = CreateTargets(render);
	if (FAILED(hr)) {
		printf("Error: GodRays CreateTargets failed\n");
		return hr;
	}

	printf("[Info] GodRays ready (%ux%u half-res) — F10 toggles\n", m_halfW, m_halfH);
	return S_OK;
}

void GodRays::Cleanup()
{
	ReleaseTargets(nullptr);
	if (m_psoComposite) { m_psoComposite->Release(); m_psoComposite = nullptr; }
	if (m_psoRays) { m_psoRays->Release(); m_psoRays = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
}

float GodRays::ProjectSun(
	Camera* camera,
	FXMVECTOR sunDirToward,
	float& outU,
	float& outV,
	float& outOcclusion)
{
	outU = 0.5f;
	outV = 0.5f;
	outOcclusion = 0.0f;

	XMMATRIX view = camera->GetView();
	XMVECTOR sunView = XMVector3TransformNormal(sunDirToward, view);
	float vx = XMVectorGetX(sunView);
	float vy = XMVectorGetY(sunView);
	float vz = XMVectorGetZ(sunView);

	if (vz <= 1e-4f)
		return 0.0f;

	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, camera->GetProjection());
	float ndcX = (vx / vz) * p._11;
	float ndcY = (vy / vz) * p._22;

	outU = ndcX * 0.5f + 0.5f;
	outV = -ndcY * 0.5f + 0.5f;

	float dx = fabsf(outU - 0.5f) * 2.0f;
	float dy = fabsf(outV - 0.5f) * 2.0f;
	float outside = fmaxf(0.0f, fmaxf(dx, dy) - 1.15f);
	float edge = 1.0f - outside * 0.35f;
	if (edge < 0.35f)
		edge = 0.35f;
	if (edge > 1.0f)
		edge = 1.0f;

	outOcclusion = edge;
	return edge;
}

void GodRays::DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
	UINT srv0, UINT srv1, UINT samplerIndex)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(pso);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(srv0));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(srv1 != UINT_MAX ? srv1 : srv0));
	cmd->SetGraphicsRootDescriptorTable(3, render->GetSamplerGpu(samplerIndex));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void GodRays::Apply(DXRender* render, Camera* camera, FXMVECTOR sunDirToward)
{
	if (!m_rootSig || !m_psoRays || !m_colorTex)
		return;

	UINT depthSrv = render->GetDepthSrvIndex();
	ID3D12Resource* backBuf = render->GetBackBuffer();
	if (depthSrv == UINT_MAX || !backBuf)
		return;

	float sunU, sunV, sunOcc;
	if (ProjectSun(camera, sunDirToward, sunU, sunV, sunOcc) <= 0.001f)
		return;

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

	GodRaysCB cb;
	cb.LightPosUV = XMFLOAT2(sunU, sunV);
	cb.Exposure = EXPOSURE;
	cb.Decay = DECAY;
	cb.Density = DENSITY;
	cb.Weight = WEIGHT;
	cb.Threshold = THRESHOLD;
	cb.Intensity = INTENSITY;
	cb.DepthCutoff = DEPTH_CUTOFF;
	cb.SunOcclusion = sunOcc;
	cb.Pad = XMFLOAT2(0.0f, 0.0f);

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

	if (m_raysState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_raysTex, m_raysState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_raysState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_VIEWPORT halfVP = {};
	halfVP.Width = (float)m_halfW;
	halfVP.Height = (float)m_halfH;
	halfVP.MaxDepth = 1.0f;
	D3D12_RECT halfSc = { 0, 0, (LONG)m_halfW, (LONG)m_halfH };
	D3D12_CPU_DESCRIPTOR_HANDLE raysRtv = render->GetRtvCpu(m_raysRtv);
	float clearRays[4] = { 0, 0, 0, 0 };
	cmd->OMSetRenderTargets(1, &raysRtv, FALSE, nullptr);
	cmd->ClearRenderTargetView(raysRtv, clearRays, 0, nullptr);
	cmd->RSSetViewports(1, &halfVP);
	cmd->RSSetScissorRects(1, &halfSc);
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoRays, m_colorSrv, depthSrv, m_linearSampler);

	render->Transition(m_raysTex, m_raysState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_raysState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	render->BindBackBufferOnly();
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoComposite, m_raysSrv, UINT_MAX, m_linearSampler);

	/* Stay on resolved back buffer (PostFX follows); do not rebind MSAA scene. */
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
