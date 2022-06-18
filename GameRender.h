#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

class GameRender
{
private:
	ID3D11Device *g_pd3dDevice;
	ID3D11DeviceContext *g_pImmediateContext;

	IDXGISwapChain *g_pSwapChain;
	ID3D11RenderTargetView *g_pRenderTargetView;

	void InitViewport(HWND hWnd);
	HRESULT CreateBackBuffer();

public:
	HRESULT Init(HWND hWnd);
	void Cleanup();

	void RenderStart();
	void RenderEnd();

	ID3D11Device *getDevice();
	ID3D11DeviceContext *getDeviceContext();
};

