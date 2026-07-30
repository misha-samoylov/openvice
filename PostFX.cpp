#include "PostFX.h"

#include <stdio.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

struct PostFXColourCB
{
	XMFLOAT4 BlurColor;
};

struct PostFXBlitCB
{
	XMFLOAT4 Color;
	XMFLOAT2 UVOffset;
	XMFLOAT2 Pad;
};

static D3D12_BLEND_DESC MakeOpaque()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_BLEND_DESC MakeAlpha()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].BlendEnable = TRUE;
	b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	b.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static D3D12_BLEND_DESC MakeAdditive()
{
	D3D12_BLEND_DESC b = {};
	b.RenderTarget[0].BlendEnable = TRUE;
	b.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	b.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return b;
}

static HRESULT CreatePostPso(
	ID3D12Device* device, ID3D12RootSignature* rootSig,
	ID3DBlob* vs, ID3DBlob* ps, const D3D12_BLEND_DESC& blend,
	ID3D12PipelineState** outPso)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = rootSig;
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.BlendState = blend;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.RasterizerState.DepthClipEnable = TRUE;
	pso.DepthStencilState.DepthEnable = FALSE;
	pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso.SampleDesc.Count = 1;
	return device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(outPso));
}

const char* PostFX::ModeName(Mode mode)
{
	switch (mode) {
	case MODE_OFF: return "OFF";
	case MODE_COLOUR_FILTER: return "NORMAL (colour filter)";
	case MODE_MOTION_BLUR: return "NORMAL (motion blur)";
	default: return "?";
	}
}

HRESULT PostFX::CreateTargets(DXRender* render)
{
	ReleaseTargets(render);

	m_width = render->GetBackBufferWidth();
	m_height = render->GetBackBufferHeight();

	HRESULT hr = render->CreateTexture2D(
		m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST,
		&m_backTex);
	if (FAILED(hr))
		return hr;
	m_backState = D3D12_RESOURCE_STATE_COPY_DEST;
	m_backSrv = render->CreateTextureSrv(m_backTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	if (m_backSrv == UINT_MAX)
		return E_FAIL;

	hr = render->CreateTexture2D(
		m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST,
		&m_frontTex);
	if (FAILED(hr))
		return hr;
	m_frontState = D3D12_RESOURCE_STATE_COPY_DEST;
	m_frontSrv = render->CreateTextureSrv(m_frontTex, DXGI_FORMAT_R8G8B8A8_UNORM);
	return (m_frontSrv != UINT_MAX) ? S_OK : E_FAIL;
}

void PostFX::ReleaseTargets(DXRender* render)
{
	if (m_frontTex) {
		if (render)
			render->DeferRelease(m_frontTex);
		else
			m_frontTex->Release();
		m_frontTex = nullptr;
	}
	if (m_backTex) {
		if (render)
			render->DeferRelease(m_backTex);
		else
			m_backTex->Release();
		m_backTex = nullptr;
	}
	m_backSrv = m_frontSrv = UINT_MAX;
}

HRESULT PostFX::Init(DXRender* render)
{
	m_rootSig = nullptr;
	m_psoColour = nullptr;
	m_psoBlitOpaque = nullptr;
	m_psoBlitAlpha = nullptr;
	m_psoBlitAdditive = nullptr;
	m_pointSampler = UINT_MAX;
	m_backTex = nullptr;
	m_frontTex = nullptr;
	m_backSrv = m_frontSrv = UINT_MAX;
	m_width = m_height = 0;
	m_mode = MODE_OFF;
	m_justInitialised = true;
	m_blurR = DEFAULT_R;
	m_blurG = DEFAULT_G;
	m_blurB = DEFAULT_B;
	m_blurAlpha = DEFAULT_BLUR_ALPHA;
	m_intensity = INTENSITY;

	ID3D12Device* device = render->GetDevice();
	HRESULT hr;

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
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &srvRange;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &sampRange;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 3;
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
	ID3DBlob* psColour = nullptr;
	ID3DBlob* psBlit = nullptr;
	hr = D3DReadFileToBlob(L"ssao_vs.cso", &vs);
	if (FAILED(hr)) { printf("Error: PostFX cannot read ssao_vs.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"colourfilter_vc_ps.cso", &psColour);
	if (FAILED(hr)) { printf("Error: PostFX cannot read colourfilter_vc_ps.cso\n"); return hr; }
	hr = D3DReadFileToBlob(L"postfx_blit_ps.cso", &psBlit);
	if (FAILED(hr)) { printf("Error: PostFX cannot read postfx_blit_ps.cso\n"); return hr; }

	hr = CreatePostPso(device, m_rootSig, vs, psColour, MakeOpaque(), &m_psoColour);
	if (FAILED(hr)) return hr;
	hr = CreatePostPso(device, m_rootSig, vs, psBlit, MakeOpaque(), &m_psoBlitOpaque);
	if (FAILED(hr)) return hr;
	hr = CreatePostPso(device, m_rootSig, vs, psBlit, MakeAlpha(), &m_psoBlitAlpha);
	if (FAILED(hr)) return hr;
	hr = CreatePostPso(device, m_rootSig, vs, psBlit, MakeAdditive(), &m_psoBlitAdditive);
	vs->Release();
	psColour->Release();
	psBlit->Release();
	if (FAILED(hr))
		return hr;

	D3D12_SAMPLER_DESC sd = {};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	m_pointSampler = render->CreateSampler(sd);
	if (m_pointSampler == UINT_MAX)
		return E_FAIL;

	hr = CreateTargets(render);
	if (FAILED(hr)) {
		printf("Error: PostFX CreateTargets failed\n");
		return hr;
	}

	printf("[Info] PostFX ready (%ux%u) — F9 cycles modes\n", m_width, m_height);
	return S_OK;
}

void PostFX::Cleanup()
{
	ReleaseTargets(nullptr);
	if (m_psoBlitAdditive) { m_psoBlitAdditive->Release(); m_psoBlitAdditive = nullptr; }
	if (m_psoBlitAlpha) { m_psoBlitAlpha->Release(); m_psoBlitAlpha = nullptr; }
	if (m_psoBlitOpaque) { m_psoBlitOpaque->Release(); m_psoBlitOpaque = nullptr; }
	if (m_psoColour) { m_psoColour->Release(); m_psoColour = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
}

void PostFX::SetBlurColor(float r, float g, float b)
{
	m_blurR = r;
	m_blurG = g;
	m_blurB = b;
}

void PostFX::SetIntensity(float intensity)
{
	m_intensity = intensity;
}

void PostFX::SetBlurAlpha(float alpha)
{
	m_blurAlpha = alpha;
}

void PostFX::SetMode(Mode mode)
{
	m_mode = mode;
	if (m_mode == MODE_MOTION_BLUR)
		m_justInitialised = true;
}

PostFX::Mode PostFX::CycleMode()
{
	m_mode = (Mode)((m_mode + 1) % MODE_COUNT);
	if (m_mode == MODE_MOTION_BLUR)
		m_justInitialised = true;
	return m_mode;
}

void PostFX::CopyBackBuffer(DXRender* render, ID3D12Resource* dest, D3D12_RESOURCE_STATES& destState)
{
	ID3D12Resource* back = render->GetBackBuffer();
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();

	/* Back buffer is RENDER_TARGET during the frame; restore after copy. */
	render->Transition(back, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	if (destState != D3D12_RESOURCE_STATE_COPY_DEST) {
		render->Transition(dest, destState, D3D12_RESOURCE_STATE_COPY_DEST);
		destState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	cmd->CopyResource(dest, back);
	render->Transition(back, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	render->Transition(dest, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	destState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void PostFX::DrawFullscreen(DXRender* render, ID3D12PipelineState* pso, UINT srvIndex)
{
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(pso);
	cmd->SetGraphicsRootDescriptorTable(1, render->GetSrvGpu(srvIndex));
	cmd->SetGraphicsRootDescriptorTable(2, render->GetSamplerGpu(m_pointSampler));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
}

void PostFX::ApplyColourFilter(DXRender* render)
{
	CopyBackBuffer(render, m_backTex, m_backState);

	PostFXColourCB cb;
	float f = m_intensity;
	cb.BlurColor = XMFLOAT4(
		m_blurR * f / 255.0f,
		m_blurG * f / 255.0f,
		m_blurB * f / 255.0f,
		30.0f / 255.0f);

	D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
	void* ptr = render->AllocFrameConstants(sizeof(cb), &cbAddr);
	if (!ptr)
		return;
	memcpy(ptr, &cb, sizeof(cb));

	render->BindBackBufferOnly();
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();
	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
	DrawFullscreen(render, m_psoColour, m_backSrv);
}

void PostFX::ApplyMotionBlur(DXRender* render)
{
	float f = m_intensity;
	float r = m_blurR * f / 255.0f;
	float g = m_blurG * f / 255.0f;
	float b = m_blurB * f / 255.0f;
	float a = m_blurAlpha * f / 255.0f;
	XMFLOAT2 offsetUV(2.0f / (float)m_width, 2.0f / (float)m_height);

	if (!m_justInitialised && m_frontState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		render->BindBackBufferOnly();
		ID3D12GraphicsCommandList* cmd = render->GetCommandList();
		cmd->SetGraphicsRootSignature(m_rootSig);

		PostFXBlitCB blit;
		blit.Color = XMFLOAT4(
			(r * 2.0f > 1.0f) ? 1.0f : r * 2.0f,
			(g * 2.0f > 1.0f) ? 1.0f : g * 2.0f,
			(b * 2.0f > 1.0f) ? 1.0f : b * 2.0f,
			30.0f / 255.0f * f);
		blit.UVOffset = offsetUV;
		blit.Pad = XMFLOAT2(0, 0);

		D3D12_GPU_VIRTUAL_ADDRESS cbAddr = 0;
		void* ptr = render->AllocFrameConstants(sizeof(blit), &cbAddr);
		if (!ptr)
			return;
		memcpy(ptr, &blit, sizeof(blit));
		cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
		DrawFullscreen(render, m_psoBlitAlpha, m_frontSrv);

		blit.Color = XMFLOAT4(r, g, b, a);
		blit.UVOffset = XMFLOAT2(0, 0);
		ptr = render->AllocFrameConstants(sizeof(blit), &cbAddr);
		if (!ptr)
			return;
		memcpy(ptr, &blit, sizeof(blit));
		cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
		DrawFullscreen(render, m_psoBlitAdditive, m_frontSrv);

		blit.UVOffset = offsetUV;
		ptr = render->AllocFrameConstants(sizeof(blit), &cbAddr);
		if (!ptr)
			return;
		memcpy(ptr, &blit, sizeof(blit));
		cmd->SetGraphicsRootConstantBufferView(0, cbAddr);
		DrawFullscreen(render, m_psoBlitAdditive, m_frontSrv);
	}

	CopyBackBuffer(render, m_frontTex, m_frontState);
	m_justInitialised = false;
}

void PostFX::Apply(DXRender* render)
{
	if (!m_rootSig || m_mode == MODE_OFF)
		return;

	if (m_mode == MODE_COLOUR_FILTER) {
		if (!m_psoColour || !m_backTex)
			return;
		ApplyColourFilter(render);
	} else if (m_mode == MODE_MOTION_BLUR) {
		if (!m_psoBlitAlpha || !m_frontTex)
			return;
		ApplyMotionBlur(render);
	}

	render->RestoreMainTargets();
	render->ApplyRasterizerState();
	render->SetOpaqueState();
}
