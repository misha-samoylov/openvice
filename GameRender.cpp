#include "GameRender.hpp"

ID3D11Device *GameRender::getDevice()
{
	return m_pDevice;
}

ID3D11DeviceContext *GameRender::getDeviceContext()
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
		MessageBox(NULL, L"Cannot create backbuffer", L"Error", MB_OK);
		return hr;
	}

	hr = m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView);
	pBackBuffer->Release(); /* now that's object does not needed */
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create render target view", L"Error", MB_OK);
		return hr;
	}

	/* connect back buffer to device context */
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

	return hr;
}

HRESULT GameRender::Init(HWND hWnd)
{
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
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; /* target - back buffer */
	sd.OutputWindow = hWnd; /* attach to window */
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; /* windowed */

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
		MessageBox(NULL, L"Cannot create device", L"Error", MB_ICONERROR | MB_OK);
		return hr;
	}

	hr = CreateBackBuffer();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create back buffer", L"Error", MB_ICONERROR | MB_OK);
		return hr;
	}

	InitViewport(hWnd);

	return hr;
}

void GameRender::Cleanup()
{
	if (m_pDeviceContext) 
		m_pDeviceContext->ClearState();

	if (m_pRenderTargetView) 
		m_pRenderTargetView->Release();
	if (m_pSwapChain) 
		m_pSwapChain->Release();

	if (m_pDeviceContext) 
		m_pDeviceContext->Release();
	if (m_pDevice) 
		m_pDevice->Release();
}

void GameRender::RenderStart()
{
	/* clear back buffer */
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);
}

void GameRender::RenderEnd()
{
	/* show back buffer to screen */
	m_pSwapChain->Present(0, 0);
}