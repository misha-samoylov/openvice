#include "SSAO.h"

#include <stdio.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct SSAOCB
{
	XMFLOAT4X4 Proj;
	XMFLOAT2 InvFullSize;
	XMFLOAT2 InvHalfSize;
	float Radius;
	float Bias;
	float Intensity;
	float Power;
	float Proj33;
	float Proj43;
	float Proj11;
	float Proj22;
};

static D3D12_BLEND_DESC MakeOpaqueBlend()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_BLEND_DESC MakeMultiplyBlend()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].BlendEnable = TRUE;
	b.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
	b.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_RASTERIZER_DESC MakeFSRaster()
{
	D3D12_RASTERIZER_DESC r = {};
	r.FillMode = D3D12_FILL_MODE_SOLID;
	r.CullMode = D3D12_CULL_MODE_NONE;
	r.DepthClipEnable = TRUE;
	return r;
}

static D3D12_DEPTH_STENCIL_DESC MakeDepthOff()
{
	D3D12_DEPTH_STENCIL_DESC d = {};
	d.DepthEnable = FALSE;
	d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	d.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	return d;
}

static HRESULT CreateFullscreenPso(
	ID3D12Device* device,
	ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps,
	const D3D12_BLEND_DESC& blend,
	DXGI_FORMAT rtvFormat,
	ID3D12PipelineState** outPso)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = rootSig;
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.BlendState = blend;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState = MakeFSRaster();
	pso.DepthStencilState = MakeDepthOff();
	pso.InputLayout = { nullptr, 0 };
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = rtvFormat;
	pso.SampleDesc.Count = 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

HRESULT SSAO::CreateHalfResTargets(DXRender* render)
{
	ReleaseHalfResTargets(render);

	m_fullW = render->GetBackBufferWidth();
	m_fullH = render->GetBackBufferHeight();
	m_halfW = (m_fullW > 1) ? (m_fullW / 2) : 1;
	m_halfH = (m_fullH > 1) ? (m_fullH / 2) : 1;

	HRESULT hr = render->CreateTexture2D(
		m_halfW, m_halfH, DXGI_FORMAT_R8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_aoTex);
	if (FAILED(hr))
		return hr;
	m_aoState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_aoRtv = render->AllocRtvIndex();
	if (m_aoRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_aoTex, nullptr, render->GetRtvCpu(m_aoRtv));
	m_aoSrv = render->CreateTextureSrv(m_aoTex, DXGI_FORMAT_R8_UNORM);
	if (m_aoSrv == UINT_MAX)
		return E_FAIL;

	hr = render->CreateTexture2D(
		m_halfW, m_halfH, DXGI_FORMAT_R8_UNORM,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&m_blurTex);
	if (FAILED(hr))
		return hr;
	m_blurState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_blurRtv = render->AllocRtvIndex();
	if (m_blurRtv == UINT_MAX)
		return E_FAIL;
	render->GetDevice()->CreateRenderTargetView(m_blurTex, nullptr, render->GetRtvCpu(m_blurRtv));
	m_blurSrv = render->CreateTextureSrv(m_blurTex, DXGI_FORMAT_R8_UNORM);
	return (m_blurSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void SSAO::ReleaseHalfResTargets(DXRender* render)
{
	if (m_blurTex) {
		if (render)
			render->DeferRelease(m_blurTex);
		else
			m_blurTex->Release();
		m_blurTex = nullptr;
	}
	if (m_aoTex) {
		if (render)
			render->DeferRelease(m_aoTex);
		else
			m_aoTex->Release();
		m_aoTex = nullptr;
	}
	m_aoRtv = m_aoSrv = m_blurRtv = m_blurSrv = UINT_MAX;
}

HRESULT SSAO::Init(DXRender* render)
{
	m_rootSig = nullptr;
	m_psoAO = nullptr;
	m_psoBlur = nullptr;
	m_psoComposite = nullptr;
	m_pointSampler = UINT_MAX;
	m_linearSampler = UINT_MAX;
	m_aoTex = nullptr;
	m_blurTex = nullptr;
	m_aoRtv = m_aoSrv = m_blurRtv = m_blurSrv = UINT_MAX;
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
	sampRange.NumDescriptors = 1;
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
	ID3DBlob* errBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) { printf("SSAO root sig: %s\n", (char*)errBlob->GetBufferPointer()); errBlob->Release(); }
		return hr;
	}
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psAO = nullptr;
	ID3DBlob* psBlur = nullptr;
	ID3DBlob* psComp = nullptr;

	hr = D3DReadFileToBlob(L"ssao_vs.cso", &vsBlob);
	if (FAILED(hr)) { printf("Error: SSAO cannot read ssao_vs.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"ssao_ps.cso", &psAO);
	if (FAILED(hr)) { printf("Error: SSAO cannot read ssao_ps.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"ssao_blur_ps.cso", &psBlur);
	if (FAILED(hr)) { printf("Error: SSAO cannot read ssao_blur_ps.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"ssao_composite_ps.cso", &psComp);
	if (FAILED(hr)) { printf("Error: SSAO cannot read ssao_composite_ps.cso\n"); return hr; }

	hr = CreateFullscreenPso(device, m_rootSig, vsBlob, psAO, MakeOpaqueBlend(), DXGI_FORMAT_R8_UNORM, &m_psoAO);
	if (FAILED(hr)) return hr;
	hr = CreateFullscreenPso(device, m_rootSig, vsBlob, psBlur, MakeOpaqueBlend(), DXGI_FORMAT_R8_UNORM, &m_psoBlur);
	if (FAILED(hr)) return hr;
	hr = CreateFullscreenPso(device, m_rootSig, vsBlob, psComp, MakeMultiplyBlend(), DXGI_FORMAT_R8G8B8A8_UNORM, &m_psoComposite);
	vsBlob->Release();
	psAO->Release();
	psBlur->Release();
	psComp->Release();
	if (FAILED(hr))
		return hr;

	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_pointSampler = render->CreateSampler(sd);
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	m_linearSampler = render->CreateSampler(sd);
	if (m_pointSampler == UINT_MAX || m_linearSampler == UINT_MAX)
		return E_FAIL;

	hr = CreateHalfResTargets(render);
	if (FAILED(hr)) {
		printf("Error: SSAO CreateHalfResTargets failed\n");
		return hr;
	}

	printf("[Info] SSAO ready (%ux%u half-res)\n", m_halfW, m_halfH);
	return S_OK;
}

void SSAO::Cleanup()
{
	ReleaseHalfResTargets(nullptr);
	if (m_psoComposite) { m_psoComposite->Release(); m_psoComposite = nullptr; }
	if (m_psoBlur) { m_psoBlur->Release(); m_psoBlur = nullptr; }
	if (m_psoAO) { m_psoAO->Release(); m_psoAO = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
}

void SSAO::DrawFullscreen(DXRender* render, ID3D12PipelineState* pso,
	UINT srv0, UINT srv1, UINT samplerIndex)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(pso);
	/* Separate tables — never rewrite shader-visible descriptors while GPU is in flight. */
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(srv0));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(srv1 != UINT_MAX ? srv1 : srv0));
	cmd->SetGraphicsRootDescriptorTable(3, render->GetSamplerGpu(samplerIndex));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void SSAO::Apply(DXRender* render, Camera* camera)
{
	if (!m_rootSig || render->GetDepthSrvIndex() == UINT_MAX)
		return;

	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	UINT depthSrv = render->GetDepthSrvIndex();

	SSAOCB cb;
	XMMATRIX proj = camera->GetProjection();
	XMStoreFloat4x4(&cb.Proj, XMMatrixTranspose(proj));
	XMFLOAT4X4 p;
	XMStoreFloat4x4(&p, proj);
	cb.InvFullSize = XMFLOAT2(1.0f / (float)m_fullW, 1.0f / (float)m_fullH);
	cb.InvHalfSize = XMFLOAT2(1.0f / (float)m_halfW, 1.0f / (float)m_halfH);
	cb.Radius = RADIUS;
	cb.Bias = BIAS;
	cb.Intensity = INTENSITY;
	cb.Power = POWER;
	cb.Proj33 = p._33;
	cb.Proj43 = p._43;
	cb.Proj11 = p._11;
	cb.Proj22 = p._22;

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* cbPtr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!cbPtr)
		return;
	memcpy(cbPtr, &cb, sizeof(cb));

	D3D12_VIEWPORT halfVP = {};
	halfVP.Width = (float)m_halfW;
	halfVP.Height = (float)m_halfH;
	halfVP.MaxDepth = 1.0f;
	D3D12_RECT halfSc = { 0, 0, (LONG)m_halfW, (LONG)m_halfH };

	/* ---- AO pass ---- */
	if (m_aoState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_aoTex, m_aoState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_aoState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE aoRtv = render->GetRtvCpu(m_aoRtv);
	cmd->OMSetRenderTargets(1, &aoRtv, FALSE, nullptr);
	cmd->RSSetViewports(1, &halfVP);
	cmd->RSSetScissorRects(1, &halfSc);
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoAO, depthSrv, UINT_MAX, m_pointSampler);

	/* ---- Blur pass ---- */
	render->Transition(m_aoTex, m_aoState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_aoState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (m_blurState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		render->Transition(m_blurTex, m_blurState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_blurState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE blurRtv = render->GetRtvCpu(m_blurRtv);
	cmd->OMSetRenderTargets(1, &blurRtv, FALSE, nullptr);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoBlur, m_aoSrv, depthSrv, m_pointSampler);

	/* ---- Composite multiply onto back buffer ---- */
	render->Transition(m_blurTex, m_blurState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_blurState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	render->BindColorTargetOnly();
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoComposite, m_blurSrv, UINT_MAX, m_linearSampler);

	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
