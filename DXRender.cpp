#include "DXRender.hpp"

#include <string.h>

ID3D11Device *DXRender::GetDevice()
{
	return m_pDevice;
}

ID3D11DeviceContext *DXRender::GetDeviceContext()
{
	return m_pDeviceContext;
}

HRESULT DXRender::CreateRasterizerStates()
{
	HRESULT hr;
	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_FRONT;
	desc.DepthClipEnable = TRUE;
	desc.MultisampleEnable = TRUE;
	hr = m_pDevice->CreateRasterizerState(&desc, &m_pRSCullFront);
	if (FAILED(hr))
		return hr;

	desc.CullMode = D3D11_CULL_NONE;
	hr = m_pDevice->CreateRasterizerState(&desc, &m_pRSCullNone);
	if (FAILED(hr))
		return hr;

	desc.FillMode = D3D11_FILL_WIREFRAME;
	desc.CullMode = D3D11_CULL_NONE;
	hr = m_pDevice->CreateRasterizerState(&desc, &m_pRSWireframe);
	if (FAILED(hr))
		return hr;

	m_pRasterizerState = m_pRSCullFront;
	m_pDeviceContext->RSSetState(m_pRasterizerState);
	return S_OK;
}

HRESULT DXRender::ChangeRasterizerStateToWireframe()
{
	m_pRasterizerState = m_pRSWireframe;
	m_pDeviceContext->RSSetState(m_pRasterizerState);
	return S_OK;
}

HRESULT DXRender::ChangeRasterizerStateToSolid()
{
	m_pRasterizerState = m_pRSCullFront;
	m_pDeviceContext->RSSetState(m_pRasterizerState);
	return S_OK;
}

void DXRender::SetCullNone()
{
	/* Keep F1 wireframe when vehicle temporarily requests cull-none. */
	if (m_pRasterizerState == m_pRSWireframe)
		m_pDeviceContext->RSSetState(m_pRSWireframe);
	else
		m_pDeviceContext->RSSetState(m_pRSCullNone);
}

void DXRender::SetCullFront()
{
	if (m_pRasterizerState == m_pRSWireframe)
		m_pDeviceContext->RSSetState(m_pRSWireframe);
	else
		m_pDeviceContext->RSSetState(m_pRSCullFront);
}

void DXRender::SetVehicleRasterizer(bool wireframe)
{
	m_pDeviceContext->RSSetState(wireframe ? m_pRSWireframe : m_pRSCullNone);
}

void DXRender::ApplyRasterizerState()
{
	if (m_pRasterizerState)
		m_pDeviceContext->RSSetState(m_pRasterizerState);
}

void DXRender::InitViewport(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	m_width = rc.right - rc.left;
	m_height = rc.bottom - rc.top;

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	/* connect viewport to device context */
	UINT countViewports = 1;
	m_pDeviceContext->RSSetViewports(countViewports, &vp);
}

void DXRender::RestoreMainTargets()
{
	m_pDeviceContext->OMSetRenderTargets(1, &m_pSceneRTV, m_pDepthStencilView);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pDeviceContext->RSSetViewports(1, &vp);
}

void DXRender::BindColorTargetOnly()
{
	m_pDeviceContext->OMSetRenderTargets(1, &m_pSceneRTV, nullptr);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pDeviceContext->RSSetViewports(1, &vp);
}

void DXRender::BindBackBufferOnly()
{
	m_pDeviceContext->OMSetRenderTargets(1, &m_pBackBufferRTV, nullptr);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pDeviceContext->RSSetViewports(1, &vp);
}

void DXRender::ResolveMSAA()
{
	if (m_msaaCount <= 1 || !m_ownsSceneColor)
		return;

	m_pDeviceContext->ResolveSubresource(
		m_pBackBuffer, 0,
		m_pSceneColor, 0,
		DXGI_FORMAT_R8G8B8A8_UNORM);
}

void DXRender::ResolveDepthForSSAO()
{
	if (m_msaaCount <= 1 || !m_pDepthResolvePS || !m_pDepthMSAA_SRV)
		return;

	ID3D11RenderTargetView* nullRTV = nullptr;
	m_pDeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);

	m_pDeviceContext->OMSetRenderTargets(1, &m_pResolvedDepthRTV, nullptr);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pDeviceContext->RSSetViewports(1, &vp);

	float blendFactor[4] = { 0, 0, 0, 0 };
	m_pDeviceContext->OMSetBlendState(m_pBlendStateOpaque, blendFactor, 0xffffffff);
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateSoftAlpha, 0); /* depth write off; no DSV bound */
	m_pDeviceContext->RSSetState(m_pRSCullNone);

	m_pDeviceContext->IASetInputLayout(nullptr);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = 0, offset = 0;
	ID3D11Buffer* nullVB = nullptr;
	m_pDeviceContext->IASetVertexBuffers(0, 1, &nullVB, &stride, &offset);
	m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	m_pDeviceContext->VSSetShader(m_pDepthResolveVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pDepthResolvePS, nullptr, 0);
	m_pDeviceContext->PSSetShaderResources(0, 1, &m_pDepthMSAA_SRV);
	m_pDeviceContext->Draw(3, 0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pDeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
	ApplyRasterizerState();
	SetOpaqueState();
}

UINT DXRender::PickMSAACount(DXGI_FORMAT format)
{
	UINT quality = 0;
	HRESULT hr = m_pDevice->CheckMultisampleQualityLevels(format, kDesiredMSAA, &quality);
	if (SUCCEEDED(hr) && quality > 0)
		return kDesiredMSAA;

	hr = m_pDevice->CheckMultisampleQualityLevels(format, 2, &quality);
	if (SUCCEEDED(hr) && quality > 0)
		return 2;

	return 1;
}

HRESULT DXRender::CreateBackBuffer()
{
	HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_pBackBuffer);

	if (FAILED(hr)) {
		printf("Error: cannot create backbuffer\n");
		m_pBackBuffer = nullptr;
		return hr;
	}

	hr = m_pDevice->CreateRenderTargetView(m_pBackBuffer, NULL, &m_pBackBufferRTV);

	if (FAILED(hr)) {
		printf("Error: cannot create render target view\n");
		m_pBackBuffer->Release();
		m_pBackBuffer = nullptr;
		return hr;
	}

	return hr;
}

HRESULT DXRender::CreateMSAAColor()
{
	m_ownsSceneColor = false;
	m_pSceneColor = nullptr;
	m_pSceneRTV = nullptr;

	if (m_msaaCount <= 1) {
		m_pSceneColor = m_pBackBuffer;
		m_pSceneRTV = m_pBackBufferRTV;
		return S_OK;
	}

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = m_width;
	td.Height = m_height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = m_msaaCount;
	td.SampleDesc.Quality = m_msaaQuality;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET;

	HRESULT hr = m_pDevice->CreateTexture2D(&td, nullptr, &m_pSceneColor);
	if (FAILED(hr))
		return hr;

	hr = m_pDevice->CreateRenderTargetView(m_pSceneColor, nullptr, &m_pSceneRTV);
	if (FAILED(hr)) {
		m_pSceneColor->Release();
		m_pSceneColor = nullptr;
		return hr;
	}

	m_ownsSceneColor = true;
	return S_OK;
}

HRESULT DXRender::CreateDepthResolveResources()
{
	m_ownsResolvedDepth = false;
	m_pResolvedDepth = nullptr;
	m_pResolvedDepthRTV = nullptr;
	m_pDepthMSAA_SRV = nullptr;
	m_pDepthResolveVS = nullptr;
	m_pDepthResolvePS = nullptr;

	if (m_msaaCount <= 1) {
		/* Single-sample depth is already an SRV. */
		m_ownsResolvedDepth = false;
		return S_OK;
	}

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.Width = m_width;
	td.Height = m_height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R32_FLOAT;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = m_pDevice->CreateTexture2D(&td, nullptr, &m_pResolvedDepth);
	if (FAILED(hr))
		return hr;

	hr = m_pDevice->CreateRenderTargetView(m_pResolvedDepth, nullptr, &m_pResolvedDepthRTV);
	if (FAILED(hr))
		return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	hr = m_pDevice->CreateShaderResourceView(m_pResolvedDepth, &srvDesc, &m_pDepthSRV);
	if (FAILED(hr))
		return hr;

	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	hr = m_pDevice->CreateShaderResourceView(m_pDepthStencil, &srvDesc, &m_pDepthMSAA_SRV);
	if (FAILED(hr))
		return hr;

	/* Fullscreen triangle VS + MSAA depth sample-0 resolve PS (sample count baked in). */
	char resolvePS[512];
	sprintf_s(resolvePS,
		"Texture2DMS<float, %u> DepthMS : register(t0);\n"
		"float4 main(float4 pos : SV_POSITION) : SV_TARGET {\n"
		"  return DepthMS.Load(int2(pos.xy), 0).r;\n"
		"}\n",
		m_msaaCount);

	static const char* resolveVS =
		"struct VSOut { float4 pos : SV_POSITION; };\n"
		"VSOut main(uint id : SV_VertexID) {\n"
		"  VSOut o;\n"
		"  float2 uv = float2((id << 1) & 2, id & 2);\n"
		"  o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
		"  return o;\n"
		"}\n";

	ID3DBlob* blob = nullptr;
	ID3DBlob* err = nullptr;
	hr = D3DCompile(resolveVS, strlen(resolveVS), nullptr, nullptr, nullptr,
		"main", "vs_5_0", 0, 0, &blob, &err);
	if (FAILED(hr)) {
		if (err) {
			printf("Error: depth resolve VS: %s\n", (char*)err->GetBufferPointer());
			err->Release();
		}
		return hr;
	}
	hr = m_pDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_pDepthResolveVS);
	blob->Release();
	if (FAILED(hr))
		return hr;

	hr = D3DCompile(resolvePS, strlen(resolvePS), nullptr, nullptr, nullptr,
		"main", "ps_5_0", 0, 0, &blob, &err);
	if (FAILED(hr)) {
		if (err) {
			printf("Error: depth resolve PS: %s\n", (char*)err->GetBufferPointer());
			err->Release();
		}
		return hr;
	}
	hr = m_pDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_pDepthResolvePS);
	blob->Release();
	if (FAILED(hr))
		return hr;

	m_ownsResolvedDepth = true;
	return S_OK;
}

HRESULT DXRender::CreateDepthStencil(HWND hWnd)
{
	HRESULT hr;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	/*
	 * Typeless depth so the same buffer can be a DSV while drawing and an SRV
	 * for SSAO / other post effects (unbind DSV before sampling).
	 */
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
	descDepth.SampleDesc.Count = m_msaaCount;
	descDepth.SampleDesc.Quality = m_msaaQuality;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	hr = m_pDevice->CreateTexture2D(&descDepth, NULL, &m_pDepthStencil);
	if (FAILED(hr))
		return hr;

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	if (m_msaaCount > 1)
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	else {
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;
	}

	hr = m_pDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	if (m_msaaCount <= 1) {
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		hr = m_pDevice->CreateShaderResourceView(m_pDepthStencil, &srvDesc, &m_pDepthSRV);
	}
	return hr;
}

HRESULT DXRender::CreateBlendStates()
{
	HRESULT hr;

	D3D11_BLEND_DESC opaqueDesc;
	ZeroMemory(&opaqueDesc, sizeof(opaqueDesc));
	opaqueDesc.RenderTarget[0].BlendEnable = FALSE;
	opaqueDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_pDevice->CreateBlendState(&opaqueDesc, &m_pBlendStateOpaque);
	if (FAILED(hr))
		return hr;

	D3D11_RENDER_TARGET_BLEND_DESC rtbd;
	ZeroMemory(&rtbd, sizeof(rtbd));
	rtbd.BlendEnable = TRUE;
	rtbd.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	rtbd.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.BlendOp = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha = D3D11_BLEND_ONE;
	rtbd.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.RenderTarget[0] = rtbd;

	hr = m_pDevice->CreateBlendState(&blendDesc, &m_pBlendStateTransparency);
	return hr;
}

HRESULT DXRender::CreateDepthStencilStates()
{
	HRESULT hr;

	D3D11_DEPTH_STENCIL_DESC opaqueDs;
	ZeroMemory(&opaqueDs, sizeof(opaqueDs));
	opaqueDs.DepthEnable = TRUE;
	opaqueDs.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	opaqueDs.DepthFunc = D3D11_COMPARISON_LESS;
	hr = m_pDevice->CreateDepthStencilState(&opaqueDs, &m_pDepthStateOpaque);
	if (FAILED(hr))
		return hr;

	/* Cutout alpha (trees/fences): solid texels write depth after PS clip. */
	D3D11_DEPTH_STENCIL_DESC cutoutDs;
	ZeroMemory(&cutoutDs, sizeof(cutoutDs));
	cutoutDs.DepthEnable = TRUE;
	cutoutDs.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	cutoutDs.DepthFunc = D3D11_COMPARISON_LESS;
	hr = m_pDevice->CreateDepthStencilState(&cutoutDs, &m_pDepthStateCutout);
	if (FAILED(hr))
		return hr;

	/* Soft alpha (glass): test only — do not occlude other translucent geometry. */
	D3D11_DEPTH_STENCIL_DESC softDs;
	ZeroMemory(&softDs, sizeof(softDs));
	softDs.DepthEnable = TRUE;
	softDs.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	softDs.DepthFunc = D3D11_COMPARISON_LESS;
	hr = m_pDevice->CreateDepthStencilState(&softDs, &m_pDepthStateSoftAlpha);
	return hr;
}

void DXRender::SetOpaqueState()
{
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pDeviceContext->OMSetBlendState(m_pBlendStateOpaque, blendFactor, 0xffffffff);
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateOpaque, 0);
}

void DXRender::SetCutoutAlphaState()
{
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pDeviceContext->OMSetBlendState(m_pBlendStateTransparency, blendFactor, 0xffffffff);
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateCutout, 0);
}

void DXRender::SetSoftAlphaState()
{
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pDeviceContext->OMSetBlendState(m_pBlendStateTransparency, blendFactor, 0xffffffff);
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateSoftAlpha, 0);
}

HRESULT DXRender::Init(HWND hWnd, bool vsync)
{
	m_vsync = vsync;
	m_width = 0;
	m_height = 0;
	m_msaaCount = 1;
	m_msaaQuality = 0;
	m_ownsSceneColor = false;
	m_ownsResolvedDepth = false;
	m_pRasterizerState = nullptr;
	m_pRSCullFront = nullptr;
	m_pRSCullNone = nullptr;
	m_pRSWireframe = nullptr;
	m_pDepthStencil = nullptr;
	m_pDepthStencilView = nullptr;
	m_pDepthSRV = nullptr;
	m_pDepthMSAA_SRV = nullptr;
	m_pResolvedDepth = nullptr;
	m_pResolvedDepthRTV = nullptr;
	m_pDepthResolveVS = nullptr;
	m_pDepthResolvePS = nullptr;
	m_pBackBuffer = nullptr;
	m_pBackBufferRTV = nullptr;
	m_pSceneColor = nullptr;
	m_pSceneRTV = nullptr;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;
	m_width = width;
	m_height = height;

	/* Non-MSAA swap chain — scene renders to an MSAA offscreen target, then resolves here. */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};

	UINT arraySize = ARRAYSIZE(featureLevels);

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		featureLevels,
		arraySize,
		D3D11_SDK_VERSION,
		&sd,
		&m_pSwapChain,
		&m_pDevice,
		NULL,
		&m_pDeviceContext
	);

	if (FAILED(hr)) {
		printf("Error: cannot CreateDeviceAndSwapChain\n");
		return hr;
	}

	m_msaaCount = PickMSAACount(DXGI_FORMAT_R8G8B8A8_UNORM);
	m_msaaQuality = 0;
	printf("[Info] MSAA %ux\n", m_msaaCount);

	hr = CreateBackBuffer();

	if (FAILED(hr)) {
		printf("Error: cannot CreateBackBuffer\n");
		return hr;
	}

	hr = CreateMSAAColor();

	if (FAILED(hr)) {
		printf("Error: cannot CreateMSAAColor\n");
		return hr;
	}

	hr = CreateDepthStencil(hWnd);

	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthStencil\n");
		return hr;
	}

	hr = CreateDepthResolveResources();

	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthResolveResources\n");
		return hr;
	}

	hr = CreateBlendStates();

	if (FAILED(hr)) {
		printf("Error: cannot CreateBlendStates\n");
		return hr;
	}

	hr = CreateDepthStencilStates();

	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthStencilStates\n");
		return hr;
	}

	m_pDeviceContext->OMSetRenderTargets(1, &m_pSceneRTV, m_pDepthStencilView);

	InitViewport(hWnd);

	hr = CreateRasterizerStates();

	if (FAILED(hr)) {
		printf("Error: cannot CreateRasterizerStates\n");
		return hr;
	}

	return hr;
}

void DXRender::Cleanup()
{
	if (m_pDeviceContext) 
		m_pDeviceContext->ClearState();

	if (m_pDepthResolvePS) {
		m_pDepthResolvePS->Release();
		m_pDepthResolvePS = nullptr;
	}
	if (m_pDepthResolveVS) {
		m_pDepthResolveVS->Release();
		m_pDepthResolveVS = nullptr;
	}
	if (m_pDepthMSAA_SRV) {
		m_pDepthMSAA_SRV->Release();
		m_pDepthMSAA_SRV = nullptr;
	}
	if (m_ownsResolvedDepth && m_pDepthSRV) {
		m_pDepthSRV->Release();
		m_pDepthSRV = nullptr;
	} else if (!m_ownsResolvedDepth && m_pDepthSRV) {
		m_pDepthSRV->Release();
		m_pDepthSRV = nullptr;
	}
	if (m_pResolvedDepthRTV) {
		m_pResolvedDepthRTV->Release();
		m_pResolvedDepthRTV = nullptr;
	}
	if (m_pResolvedDepth) {
		m_pResolvedDepth->Release();
		m_pResolvedDepth = nullptr;
	}

	if (m_pDepthStencilView) {
		m_pDepthStencilView->Release();
		m_pDepthStencilView = nullptr;
	}

	if (m_pDepthStencil) {
		m_pDepthStencil->Release();
		m_pDepthStencil = nullptr;
	}

	if (m_ownsSceneColor) {
		if (m_pSceneRTV) {
			m_pSceneRTV->Release();
			m_pSceneRTV = nullptr;
		}
		if (m_pSceneColor) {
			m_pSceneColor->Release();
			m_pSceneColor = nullptr;
		}
	} else {
		m_pSceneRTV = nullptr;
		m_pSceneColor = nullptr;
	}

	if (m_pBackBufferRTV) {
		m_pBackBufferRTV->Release();
		m_pBackBufferRTV = nullptr;
	}

	if (m_pBackBuffer) {
		m_pBackBuffer->Release();
		m_pBackBuffer = nullptr;
	}

	if (m_pSwapChain) 
		m_pSwapChain->Release();

	if (m_pRSCullFront)
		m_pRSCullFront->Release();
	if (m_pRSCullNone)
		m_pRSCullNone->Release();
	if (m_pRSWireframe)
		m_pRSWireframe->Release();
	m_pRasterizerState = nullptr;

	if (m_pBlendStateOpaque)
		m_pBlendStateOpaque->Release();

	if (m_pBlendStateTransparency)
		m_pBlendStateTransparency->Release();

	if (m_pDepthStateOpaque)
		m_pDepthStateOpaque->Release();

	if (m_pDepthStateCutout)
		m_pDepthStateCutout->Release();

	if (m_pDepthStateSoftAlpha)
		m_pDepthStateSoftAlpha->Release();

	if (m_pDeviceContext) 
		m_pDeviceContext->Release();

	if (m_pDevice) 
		m_pDevice->Release();
}

void DXRender::RenderStart()
{
	float clearColor[4] = { 0.49804f, 0.78431f, 0.94510f, 1.0f };
	m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV, clearColor);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	RestoreMainTargets();
	SetOpaqueState();
}

void DXRender::RenderEnd()
{
	if (m_vsync) {
		m_pSwapChain->Present(1, 0);
	}
	else {
		m_pSwapChain->Present(0, 0);
	}
}