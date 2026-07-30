#include "RtBouncePass.h"
#include "core/GameConfig.h"

#include <stdio.h>
#include <cmath>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct RtBounceCB
{
	XMFLOAT4X4 InvViewProj;
	XMFLOAT3 CamPos;
	float BounceStrength;
	XMFLOAT3 SunDir;
	float SunStrength;
	XMFLOAT3 SkyColor;
	float ShadowBias;
	float MaxRayT;
	float Pad0;
	XMFLOAT2 Pad1;
};

static HRESULT CreateFullscreenPso(
	ID3D12Device* device, ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps, DXGI_FORMAT rtvFmt,
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
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = rtvFmt;
	pso.SampleDesc.Count = 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

HRESULT RtBouncePass::CreateTargets(DXRender* render)
{
	ReleaseTargets(render);

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	/* Quarter-res keeps RayQuery cheap enough to avoid TDR. */
	m_rtW = (m_fullW > 3) ? (m_fullW / 4) : 1;
	m_rtH = (m_fullH > 3) ? (m_fullH / 4) : 1;

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
		m_rtW, m_rtH, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_rtTex);
	if (FAILED(hr))
		return hr;
	m_rtState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_rtRtv = render->AllocRtvIndex();
	if (m_rtRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_rtTex, nullptr, render->GetRtvCpu(m_rtRtv));
	m_rtSrv = render->CreateTextureSrv(m_rtTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	return (m_rtSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void RtBouncePass::ReleaseTargets(DXRender* render)
{
	if (m_rtTex) {
		if (render)
			render->DeferRelease(m_rtTex);
		else
			m_rtTex->Release();
		m_rtTex = nullptr;
	}
	if (m_colorTex) {
		if (render)
			render->DeferRelease(m_colorTex);
		else
			m_colorTex->Release();
		m_colorTex = nullptr;
	}
	m_colorSrv = m_rtRtv = m_rtSrv = UINT_MAX;
}

HRESULT RtBouncePass::Init(DXRender* render)
{
	Cleanup();
	if (!render || !render->SupportsRaytracing()) {
		printf("[Warn] RtBouncePass: no DXR — skipped\n");
		return E_FAIL;
	}

	ID3D12Device* device = render->GetDevice();

	D3D12_DESCRIPTOR_RANGE srv0 = {};
	srv0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv0.NumDescriptors = 1;
	srv0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE srv1 = {};
	srv1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv1.NumDescriptors = 1;
	srv1.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE samp = {};
	samp.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samp.NumDescriptors = 1;
	samp.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER params[5] = {};
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
	params[4].DescriptorTable.pDescriptorRanges = &samp;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 5;
	rsDesc.pParameters = params;

	ID3DBlob* sigBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr))
		return hr;
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	ID3DBlob* vs = nullptr;
	ID3DBlob* psTrace = nullptr;
	ID3DBlob* psComp = nullptr;
	hr = D3DReadFileToBlob(L"rt_fullscreen_vs.cso", &vs);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_fullscreen_vs.cso missing\n");
		return hr;
	}
	hr = D3DReadFileToBlob(L"rt_bounce_ps.cso", &psTrace);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_bounce_ps.cso missing\n");
		vs->Release();
		return hr;
	}
	hr = D3DReadFileToBlob(L"rt_bounce_composite_ps.cso", &psComp);
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass: rt_bounce_composite_ps.cso missing\n");
		vs->Release();
		psTrace->Release();
		return hr;
	}

	hr = CreateFullscreenPso(device, m_rootSig, vs, psTrace, DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoTrace);
	if (SUCCEEDED(hr))
		hr = CreateFullscreenPso(device, m_rootSig, vs, psComp, DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoComposite);
	vs->Release();
	psTrace->Release();
	psComp->Release();
	if (FAILED(hr)) {
		printf("[Error] RtBouncePass PSO failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}

	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_pointSampler = render->CreateSampler(sd);
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	m_linearSampler = render->CreateSampler(sd);
	if (m_pointSampler == UINT_MAX || m_linearSampler == UINT_MAX)
		return E_FAIL;

	hr = CreateTargets(render);
	if (FAILED(hr))
		return hr;

	printf("[Info] RtBouncePass ready (%ux%u quarter-res, 2 rays/pixel)\n", m_rtW, m_rtH);
	return S_OK;
}

void RtBouncePass::Cleanup()
{
	ReleaseTargets(nullptr);
	if (m_psoComposite) { m_psoComposite->Release(); m_psoComposite = nullptr; }
	if (m_psoTrace) { m_psoTrace->Release(); m_psoTrace = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
	m_pointSampler = m_linearSampler = UINT_MAX;
}

void RtBouncePass::DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
	UINT colorSrv, UINT depthSrv, D3D12_GPU_VIRTUAL_ADDRESS tlasVA, UINT sampler)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(pso);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(colorSrv));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(depthSrv != UINT_MAX ? depthSrv : colorSrv));
	cmd->SetGraphicsRootShaderResourceView(3, tlasVA);
	cmd->SetGraphicsRootDescriptorTable(4, render->GetSamplerGpu(sampler));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void RtBouncePass::Apply(
	DXRender* render,
	Camera* camera,
	FXMVECTOR sunDirToward,
	D3D12_GPU_VIRTUAL_ADDRESS tlasVA)
{
	if (!m_rootSig || !m_psoTrace || !m_colorTex || tlasVA == 0)
		return;

	UINT depthSrv = render->GetDepthSrvIndex();
	ID3D12Resource* backBuf = render->GetBackBuffer();
	if (depthSrv == UINT_MAX || !backBuf)
		return;

	if (m_fullW != render->GetBackBufferWidth() || m_fullH != render->GetBackBufferHeight()) {
		if (FAILED(CreateTargets(render)))
			return;
	}

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

	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	RtBounceCB cb = {};
	XMStoreFloat4x4(&cb.InvViewProj, XMMatrixTranspose(invViewProj));
	XMVECTOR cam = camera->GetPosition();
	cb.CamPos = XMFLOAT3(XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam));
	cb.BounceStrength = 0.22f;
	XMStoreFloat3(&cb.SunDir, XMVector3Normalize(sunDirToward));
	cb.SunStrength = 0.85f;
	cb.SkyColor = XMFLOAT3(SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B);
	cb.ShadowBias = 0.12f;
	cb.MaxRayT = DRAW_DISTANCE;
	cb.Pad0 = 0.0f;
	cb.Pad1 = XMFLOAT2(0.0f, 0.0f);

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

	if (m_rtState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_rtTex, m_rtState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_rtState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_VIEWPORT rtVP = {};
	rtVP.Width = (float)m_rtW;
	rtVP.Height = (float)m_rtH;
	rtVP.MaxDepth = 1.0f;
	D3D12_RECT rtSc = { 0, 0, (LONG)m_rtW, (LONG)m_rtH };
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = render->GetRtvCpu(m_rtRtv);
	float clearRt[4] = { 0, 0, 0, 0 };
	cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	cmd->ClearRenderTargetView(rtv, clearRt, 0, nullptr);
	cmd->RSSetViewports(1, &rtVP);
	cmd->RSSetScissorRects(1, &rtSc);
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoTrace, m_colorSrv, depthSrv, tlasVA, m_pointSampler);

	render->Transition(m_rtTex, m_rtState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_rtState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	render->BindBackBufferOnly();
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	/* Composite: t0=color copy, t1=RT lit, t2 unused — bind fallback TLAS anyway. */
	DrawFullscreen(render, m_psoComposite, m_colorSrv, m_rtSrv, tlasVA, m_linearSampler);

	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
