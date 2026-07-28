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

	/* Rebind scene color RTV + scene depth + main viewport (after shadow pass). */
	void RestoreMainTargets();
	/* Scene color RTV only (no depth) — SSAO composite onto MSAA target. */
	void BindColorTargetOnly();
	/* Swap-chain color only — PostFX after MSAA resolve. */
	void BindBackBufferOnly();
	/* Resolve MSAA scene color into the non-MSAA swap-chain back buffer. */
	void ResolveMSAA();
	/* Resolve MSAA depth (sample 0) into a single-sample SRV for SSAO. */
	void ResolveDepthForSSAO();

	/* Opaque: blending off, depth write on. */
	void SetOpaqueState();
	/* Cutout alpha (trees): A2C + depth write (PS clips transparent texels). */
	void SetCutoutAlphaState();
	/* Soft alpha (glass): blending on, depth write off — draw back-to-front. */
	void SetSoftAlphaState();

	ID3D11Device *GetDevice();
	ID3D11DeviceContext *GetDeviceContext();
	/* Scene depth as shader resource (unbind DSV before sampling). Single-sample. */
	ID3D11ShaderResourceView *GetDepthSRV() const { return m_pDepthSRV; }
	ID3D11RenderTargetView *GetBackBufferRTV() const { return m_pBackBufferRTV; }
	/* Non-MSAA swap-chain color — for PostFX CopyResource. */
	ID3D11Texture2D *GetBackBufferTexture() const { return m_pBackBuffer; }
	UINT GetMSAASampleCount() const { return m_msaaCount; }

	HRESULT ChangeRasterizerStateToWireframe();
	HRESULT ChangeRasterizerStateToSolid();
	void ApplyRasterizerState();

	/* Vehicles: RW winding vs Y/Z remap fights CULL_FRONT — draw both sides. */
	void SetCullNone();
	void SetCullFront();
	/* Solid or wireframe, both sides — for vehicle debug (F1). */
	void SetVehicleRasterizer(bool wireframe);

	bool IsWireframe() const { return m_pRasterizerState == m_pRSWireframe; }

	UINT GetBackBufferWidth() const { return m_width; }
	UINT GetBackBufferHeight() const { return m_height; }

private:
	static constexpr UINT kDesiredMSAA = 4;

	void InitViewport(HWND hWnd);
	HRESULT CreateBackBuffer();
	HRESULT CreateMSAAColor();
	HRESULT CreateDepthStencil(HWND hWnd);
	HRESULT CreateDepthResolveResources();
	HRESULT CreateBlendStates();
	HRESULT CreateDepthStencilStates();
	HRESULT CreateRasterizerStates();
	UINT PickMSAACount(DXGI_FORMAT format);

	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pDeviceContext;

	IDXGISwapChain *m_pSwapChain;
	/* Non-MSAA swap-chain buffer (Present + PostFX). */
	ID3D11Texture2D *m_pBackBuffer;
	ID3D11RenderTargetView *m_pBackBufferRTV;
	/* MSAA scene color (or aliases back buffer when MSAA is off). */
	ID3D11Texture2D *m_pSceneColor;
	ID3D11RenderTargetView *m_pSceneRTV;
	bool m_ownsSceneColor;

	ID3D11RasterizerState *m_pRasterizerState;
	ID3D11RasterizerState *m_pRSCullFront;
	ID3D11RasterizerState *m_pRSCullNone;
	ID3D11RasterizerState *m_pRSWireframe;

	/* MSAA (or 1x) scene depth. */
	ID3D11Texture2D* m_pDepthStencil;
	ID3D11DepthStencilView* m_pDepthStencilView;
	/* Single-sample depth for SSAO (resolved from MSAA, or same as scene at 1x). */
	ID3D11Texture2D* m_pResolvedDepth;
	ID3D11RenderTargetView* m_pResolvedDepthRTV;
	ID3D11ShaderResourceView* m_pDepthSRV;
	ID3D11ShaderResourceView* m_pDepthMSAA_SRV;
	ID3D11VertexShader* m_pDepthResolveVS;
	ID3D11PixelShader* m_pDepthResolvePS;
	bool m_ownsResolvedDepth;

	ID3D11BlendState* m_pBlendStateOpaque;
	ID3D11BlendState* m_pBlendStateTransparency;
	/* Cutout foliage/fences: MSAA alpha-to-coverage softens binary alpha edges. */
	ID3D11BlendState* m_pBlendStateCutout;

	ID3D11DepthStencilState* m_pDepthStateOpaque;
	ID3D11DepthStencilState* m_pDepthStateCutout;
	ID3D11DepthStencilState* m_pDepthStateSoftAlpha;

	UINT m_width;
	UINT m_height;
	UINT m_msaaCount;
	UINT m_msaaQuality;
	bool m_vsync;
};
