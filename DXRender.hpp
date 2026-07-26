#pragma once

#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

class DXRender
{
public:
	HRESULT Init(HWND hWnd, bool vsync);
	void Cleanup();

	void RenderStart();
	void RenderEnd();

	/* Rebind color RTV + scene depth + main viewport (after shadow pass). */
	void RestoreMainTargets();

	/* Opaque: blending off, depth write on. */
	void SetOpaqueState();
	/* Cutout alpha (trees): blending on, depth write on (PS clips transparent texels). */
	void SetCutoutAlphaState();
	/* Soft alpha (glass): blending on, depth write off — draw back-to-front. */
	void SetSoftAlphaState();

	ID3D11Device *GetDevice();
	ID3D11DeviceContext *GetDeviceContext();

	HRESULT ChangeRasterizerStateToWireframe();
	HRESULT ChangeRasterizerStateToSolid();
	void ApplyRasterizerState();

	/* Vehicles: RW winding vs Y/Z remap fights CULL_FRONT — draw both sides. */
	void SetCullNone();
	void SetCullFront();

	UINT GetBackBufferWidth() const { return m_width; }
	UINT GetBackBufferHeight() const { return m_height; }

private:
	void InitViewport(HWND hWnd);
	HRESULT CreateBackBuffer();
	
	HRESULT CreateDepthStencil(HWND hWnd);
	HRESULT CreateBlendStates();
	HRESULT CreateDepthStencilStates();
	HRESULT CreateRasterizerStates();

	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pDeviceContext;

	IDXGISwapChain *m_pSwapChain;
	ID3D11RenderTargetView *m_pRenderTargetView;

	ID3D11RasterizerState *m_pRasterizerState;
	ID3D11RasterizerState *m_pRSCullFront;
	ID3D11RasterizerState *m_pRSCullNone;
	ID3D11RasterizerState *m_pRSWireframe;

	ID3D11Texture2D* m_pDepthStencil;
	ID3D11DepthStencilView* m_pDepthStencilView;

	ID3D11BlendState* m_pBlendStateOpaque;
	ID3D11BlendState* m_pBlendStateTransparency;

	ID3D11DepthStencilState* m_pDepthStateOpaque;
	ID3D11DepthStencilState* m_pDepthStateCutout;
	ID3D11DepthStencilState* m_pDepthStateSoftAlpha;

	UINT m_width;
	UINT m_height;
	bool m_vsync;
};
