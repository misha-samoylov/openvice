#include "DXRender.hpp"

ID3D11Device *DXRender::GetDevice()
{
	return m_pDevice;
}

ID3D11DeviceContext *DXRender::GetDeviceContext()
{
	return m_pDeviceContext;
}

HRESULT DXRender::ChangeRasterizerStateToWireframe()
{
	HRESULT hr;

	D3D11_RASTERIZER_DESC wfdesc;
	ZeroMemory(&wfdesc, sizeof(D3D11_RASTERIZER_DESC));
	wfdesc.FillMode = D3D11_FILL_WIREFRAME;
	wfdesc.CullMode = D3D11_CULL_NONE;

	// ������� ������������ � ������� �����������
	hr = m_pDevice->CreateRasterizerState(&wfdesc, &m_pRasterizerState);

	// �������� ��������� ��������� ������������
	m_pDeviceContext->RSSetState(m_pRasterizerState);

	return hr;
}

void DXRender::InitViewport(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	/* connect viewport to device context */
	UINT countViewports = 1;
	m_pDeviceContext->RSSetViewports(countViewports, &vp);
}

HRESULT DXRender::CreateBackBuffer()
{
	/* front buffer - RenderTargetOutput */
	/* back buffer - RenderTargetView */
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr)) {
		printf("Error: cannot create backbuffer\n");
		return hr;
	}

	hr = m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView);
	pBackBuffer->Release(); /* now that's object does not needed */

	if (FAILED(hr)) {
		printf("Error: cannot create render target view\n");
		return hr;
	}

	return hr;
}

HRESULT DXRender::ChangeRasterizerStateToSolid()
{
	HRESULT hr;

	D3D11_RASTERIZER_DESC solidDesc;
	ZeroMemory(&solidDesc, sizeof(D3D11_RASTERIZER_DESC));
	solidDesc.FillMode = D3D11_FILL_SOLID;
	solidDesc.CullMode = D3D11_CULL_FRONT;

	// ������� ������������ � ������� �����������
	hr = m_pDevice->CreateRasterizerState(&solidDesc, &m_pRasterizerState);

	// �������� ��������� ��������� ������������
	m_pDeviceContext->RSSetState(m_pRasterizerState);

	return hr;
}

void DXRender::ApplyRasterizerState()
{
	if (m_pRasterizerState)
		m_pDeviceContext->RSSetState(m_pRasterizerState);
}

HRESULT DXRender::CreateDepthStencil(HWND hWnd)
{
	HRESULT hr;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	// ������� ��������-�������� ������ ������
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width; // ������ �
	descDepth.Height = height; // ������ ��������
	descDepth.MipLevels = 1; // ������� ������������
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // ������ (������ �������)
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; // ��� - ����� ������
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	// ��� ������ ����������� ���������-�������� ������� ������ ��������
	hr = m_pDevice->CreateTexture2D(&descDepth, NULL, &m_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// ������ ���� ������� ��� ������ ������ ������
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format; // ������ ��� � ��������
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	// ��� ������ ����������� ���������-�������� � �������� ������� ������ ������ ������
	hr = m_pDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);

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

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	/* properties front buffer and attach it to window */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; /* counts buffer = 1 */
	sd.BufferDesc.Width = width; /* buffer width */
	sd.BufferDesc.Height = height; /* buffer height */
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; /* pixel format in buffer */
	sd.BufferDesc.RefreshRate.Numerator = 60; /* screen refresh rate */
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; /* target is back buffer */
	sd.OutputWindow = hWnd; // Set window
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // Window mode

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

	hr = CreateBackBuffer();

	if (FAILED(hr)) {
		printf("Error: cannot CreateBackBuffer\n");
		return hr;
	}

	hr = CreateDepthStencil(hWnd);

	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthStencil\n");
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

	/* connect back buffer to device context */
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	InitViewport(hWnd);

	hr = ChangeRasterizerStateToSolid();

	if (FAILED(hr)) {
		printf("Error: cannot ChangeRasterizerStateToSolid\n");
		return hr;
	}

	return hr;
}

void DXRender::Cleanup()
{
	if (m_pDeviceContext) 
		m_pDeviceContext->ClearState();

	if (m_pDepthStencilView)
		m_pDepthStencilView->Release();

	if (m_pRenderTargetView) 
		m_pRenderTargetView->Release();

	if (m_pSwapChain) 
		m_pSwapChain->Release();

	if (m_pRasterizerState)
		m_pRasterizerState->Release();

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
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
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