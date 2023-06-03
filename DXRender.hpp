#pragma once

#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

class DXRender
{
public:
	HRESULT Init(HWND hWnd);
	void Cleanup();

	void RenderStart();
	void RenderEnd();

	ID3D11Device *GetDevice();
	ID3D11DeviceContext *GetDeviceContext();

	HRESULT ChangeRasterizerStateToWireframe();
	HRESULT ChangeRasterizerStateToSolid();

private:
	void InitViewport(HWND hWnd);
	HRESULT CreateBackBuffer();
	
	HRESULT CreateDepthStencil();
	HRESULT CreateBlendState();

	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pDeviceContext;

	IDXGISwapChain *m_pSwapChain;
	ID3D11RenderTargetView *m_pRenderTargetView;

	ID3D11RasterizerState *m_pRasterizerState;

	ID3D11Texture2D* m_pDepthStencil; // Текстура буфера глубин
	ID3D11DepthStencilView* m_pDepthStencilView; // Объект вида, буфер глубин

	ID3D11BlendState* Transparency;

	HWND m_hWnd;
};