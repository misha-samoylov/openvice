#pragma once

#include <stdint.h>

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * Volumetric raymarched cloud slab (half-res) + procedural sun disc/glow.
 * Clouds drawn after sky clear, before world (depth off, alpha over sky).
 */
class Clouds
{
public:
	bool Init(DXRender* render, const char* particleTxdPath);
	void Update(float dt, Camera* camera);
	/* sunDirToward = unit vector toward the sun (engine Y-up), same as ShadowMap.
	 * drawClouds=false still draws the sun, skips volumetric clouds. */
	void Render(DXRender* render, Camera* camera, FXMVECTOR sunDirToward, bool drawClouds = true);
	void Cleanup();

private:
	struct CloudVertex {
		float x, y; /* NDC */
		float u, v;
		float r, g, b, a;
	};

	struct CloudsCB {
		XMFLOAT4X4 InvViewProj;
		XMFLOAT3 CamPos;
		float Time;
		XMFLOAT3 SunDir;
		float Coverage;
		XMFLOAT3 SkyColor;
		float DensityMult;
		XMFLOAT3 CloudSilver;
		float Absorption;
		float CloudBottom;
		float CloudTop;
		float WindSpeed;
		float Ambient;
	};

	enum { MAX_QUADS = 8 };

	/* Angular size scale; glow extent is ~28 * SUN_SIZE in projected units. */
	static constexpr float SUN_SIZE = 2.5f;

	static constexpr float CLOUD_BOTTOM = 140.0f;
	static constexpr float CLOUD_TOP = 320.0f;
	static constexpr float CLOUD_COVERAGE = 0.48f;
	static constexpr float CLOUD_DENSITY = 0.055f;
	static constexpr float CLOUD_ABSORPTION = 0.55f;
	static constexpr float CLOUD_WIND = 12.0f;
	static constexpr float CLOUD_AMBIENT = 1.15f;

	bool CreatePipeline(DXRender* render);
	bool CreateStates(DXRender* render);
	bool CreateTargets(DXRender* render);
	void ReleaseTargets();

	bool ProjectGtaPoint(Camera* camera, float gtaX, float gtaY, float gtaZ,
		float screenW, float screenH,
		float* outSX, float* outSY, float* outSzx, float* outSzy) const;

	void RenderSun(DXRender* render, Camera* camera, FXMVECTOR sunDirToward,
		float screenW, float screenH);
	void RenderVolumetric(DXRender* render, Camera* camera, FXMVECTOR sunDirToward);

	void FlushBatch(DXRender* render, ID3D11BlendState* blend,
		CloudVertex* verts, int vertCount);
	void DrawFullscreen(ID3D11DeviceContext* ctx);

	ID3D11Buffer* m_vb;
	ID3D11VertexShader* m_vsSun;
	ID3D11PixelShader* m_psSun;
	ID3D11InputLayout* m_layout;

	ID3D11VertexShader* m_vsCloud;
	ID3D11PixelShader* m_psCloud;
	ID3D11PixelShader* m_psComposite;
	ID3D11Buffer* m_cb;

	ID3D11SamplerState* m_sampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthOff;
	ID3D11BlendState* m_blendAdditive;
	ID3D11BlendState* m_blendAlpha;
	ID3D11BlendState* m_blendOpaque;

	/* Half-res volumetric buffer. */
	ID3D11Texture2D* m_cloudTex;
	ID3D11RenderTargetView* m_cloudRTV;
	ID3D11ShaderResourceView* m_cloudSRV;
	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;

	float m_time;
	float m_wind;
	bool m_ready;
};
