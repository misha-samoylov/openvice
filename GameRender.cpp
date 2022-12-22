#include "GameRender.hpp"

ID3D11Device *GameRender::GetDevice()
{
	return m_pDevice;
}

ID3D11DeviceContext *GameRender::GetDeviceContext()
{
	return m_pDeviceContext;
}

void GameRender::InitViewport(HWND hWnd)
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

HRESULT GameRender::CreateBackBuffer()
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

HRESULT GameRender::CreateWireframe()
{
	HRESULT hr;

	D3D11_RASTERIZER_DESC wfdesc;
	ZeroMemory(&wfdesc, sizeof(D3D11_RASTERIZER_DESC));
	wfdesc.FillMode = D3D11_FILL_SOLID; // D3D11_FILL_WIREFRAME
	wfdesc.CullMode = D3D11_CULL_BACK; // D3D11_CULL_BACK

	hr = m_pDevice->CreateRasterizerState(&wfdesc, &m_pWireframe);

	// Включаем указанные настройки растеризации
	m_pDeviceContext->RSSetState(m_pWireframe);

	return hr;
}

HRESULT GameRender::CreateDepthStencil()
{
	HRESULT hr;

	RECT rc;
	GetClientRect(m_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	// Создаем текстуру-описание буфера глубин
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width; // ширина и
	descDepth.Height = height; // высота текстуры
	descDepth.MipLevels = 1; // уровень интерполяции
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // формат (размер пикселя)
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; // вид - буфер глубин
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	// При помощи заполненной структуры-описания создаем объект текстуры
	hr = m_pDevice->CreateTexture2D(&descDepth, NULL, &m_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Теперь надо создать сам объект буфера глубин
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format; // формат как в текстуре
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	// При помощи заполненной структуры-описания и текстуры создаем объект буфера глубин
	hr = m_pDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);

	return hr;
}

HRESULT GameRender::Init(HWND hWnd)
{
	m_hWnd = hWnd;

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
	sd.OutputWindow = hWnd; // Указываем окно
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // Оконный режим

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

	hr = CreateDepthStencil();

	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthStencil\n");
		return hr;
	}

	/* connect back buffer to device context */
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	InitViewport(hWnd);
	CreateWireframe();

	return hr;
}

void GameRender::Cleanup()
{
	// Сначала отключим контекст устройства
	if (m_pDeviceContext) 
		m_pDeviceContext->ClearState();

	// Потом удалим объекты
	if (m_pDepthStencilView) m_pDepthStencilView->Release();

	if (m_pRenderTargetView) 
		m_pRenderTargetView->Release();
	if (m_pSwapChain) 
		m_pSwapChain->Release();

	if (m_pWireframe)
		m_pWireframe->Release();

	if (m_pDeviceContext) 
		m_pDeviceContext->Release();
	if (m_pDevice) 
		m_pDevice->Release();
}

void GameRender::RenderStart()
{
	// Очищаем задний буфер
	float clearColor[4] = { 0.21, 0.25, 0.31, 1.0f };
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);

	// Очищаем буфер глубин до едицины (максимальная глубина)
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void GameRender::RenderEnd()
{
	// Показываем задний буфер на экран
	m_pSwapChain->Present(0, 0);
}