#include "GameRender.h"

ID3D11Device *GameRender::getDevice()
{
	return g_pd3dDevice;
}

ID3D11DeviceContext *GameRender::getDeviceContext()
{
	return g_pImmediateContext;
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

	// Connect viewport to device context
	UINT countViewports = 1;
	g_pImmediateContext->RSSetViewports(countViewports, &vp);
}

HRESULT GameRender::CreateBackBuffer()
{
	// RenderTargetOutput - front buffer
	// RenderTargetView - back buffer
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create backbuffer", L"Error", MB_OK);
		return hr;
	}

	// Device used for create all objects
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release(); // That's object does not needed
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create render target view", L"Error", MB_OK);
		return hr;
	}

	// Connect back buffer to device context
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	return hr;
}

HRESULT GameRender::Init(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	/* Properties front buffer and attach it to window */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; // counts buffer = 1
	sd.BufferDesc.Width = width; // width buffer
	sd.BufferDesc.Height = height; // height buffer
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // pixel format in buffer
	sd.BufferDesc.RefreshRate.Numerator = 60; // частота обновления экрана
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // назначение буфера - задний буфер
	sd.OutputWindow = hWnd; // attach to window
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // windowed

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
		&g_pSwapChain,
		&g_pd3dDevice,
		NULL,
		&g_pImmediateContext
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
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	//CleanupGeometry();

	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

void GameRender::RenderStart()
{
	// Clear back buffer
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
}

void GameRender::RenderEnd()
{
	// Show back buffer to screen
	g_pSwapChain->Present(0, 0);
}
