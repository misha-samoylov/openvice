#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * Screen-space ambient occlusion (depth-only).
 * Half-res AO → bilateral blur → multiply onto the back buffer.
 */
class SSAO
{
public:
	static constexpr float RADIUS = 1.15f;
	static constexpr float BIAS = 0.03f;
	static constexpr float INTENSITY = 1.35f;
	static constexpr float POWER = 3.6f;

	HRESULT Init(DXRender* render);
	void Cleanup();

	/* Call after the scene has written depth; multiplies AO onto color. */
	void Apply(DXRender* render, Camera* camera);

private:
	HRESULT CreateHalfResTargets(DXRender* render);
	void ReleaseHalfResTargets();
	void DrawFullscreen(ID3D11DeviceContext* ctx);

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_psAO;
	ID3D11PixelShader* m_psBlur;
	ID3D11PixelShader* m_psComposite;
	ID3D11Buffer* m_cb;
	ID3D11SamplerState* m_pointSampler;
	ID3D11SamplerState* m_linearSampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthDisabled;
	ID3D11BlendState* m_blendOpaque;
	ID3D11BlendState* m_blendMultiply;

	ID3D11Texture2D* m_aoTex;
	ID3D11RenderTargetView* m_aoRTV;
	ID3D11ShaderResourceView* m_aoSRV;

	ID3D11Texture2D* m_blurTex;
	ID3D11RenderTargetView* m_blurRTV;
	ID3D11ShaderResourceView* m_blurSRV;

	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;
};
