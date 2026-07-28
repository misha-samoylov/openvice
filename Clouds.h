#pragma once

#include <stdint.h>

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * re3 miami CClouds — camera-locked billboard sprites from PARTICLE.TXD.
 * Also draws the yellow SUN_CORE / SUN_CORONA (coronastar) like CCoronas::DoSunAndMoon.
 * Drawn after sky clear, before world (depth off).
 */
class Clouds
{
public:
	bool Init(DXRender* render, const char* particleTxdPath);
	void Update(float dt, Camera* camera);
	/* sunDirToward = unit vector toward the sun (engine Y-up), same as ShadowMap.
	 * drawClouds=false still draws the sun, skips cloud billboards. */
	void Render(DXRender* render, Camera* camera, FXMVECTOR sunDirToward, bool drawClouds = true);
	void Cleanup();

private:
	struct CloudVertex {
		float x, y; /* NDC */
		float u, v;
		float r, g, b, a;
	};

	enum {
		TEX_CLOUD1 = 0,
		TEX_CLOUD2,
		TEX_CLOUD3,
		TEX_HILIT,
		TEX_MASKED,
		TEX_CORONASTAR,
		TEX_COUNT
	};
	enum { MAX_QUADS = 64 };

	/* Midday sunny TIMECYC SunSz / core+corona RGB. */
	static constexpr float SUN_SIZE = 2.5f;
	static constexpr float SUN_CORE_R = 255.0f / 255.0f;
	static constexpr float SUN_CORE_G = 128.0f / 255.0f;
	static constexpr float SUN_CORE_B = 0.0f / 255.0f;
	static constexpr float SUN_CORONA_R = 255.0f / 255.0f;
	static constexpr float SUN_CORONA_G = 128.0f / 255.0f;
	static constexpr float SUN_CORONA_B = 0.0f / 255.0f;

	bool LoadTextures(DXRender* render, const char* particleTxdPath);
	bool CreatePipeline(DXRender* render);
	bool CreateStates(DXRender* render);

	bool ProjectGtaPoint(Camera* camera, float gtaX, float gtaY, float gtaZ,
		float screenW, float screenH,
		float* outSX, float* outSY, float* outSzx, float* outSzy) const;

	void RenderSun(DXRender* render, Camera* camera, FXMVECTOR sunDirToward,
		float screenW, float screenH);

	void FlushBatch(DXRender* render, ID3D11ShaderResourceView* srv,
		ID3D11BlendState* blend, CloudVertex* verts, int vertCount);

	ID3D11Buffer* m_vb;
	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11InputLayout* m_layout;
	ID3D11ShaderResourceView* m_textures[TEX_COUNT];
	ID3D11SamplerState* m_sampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthOff;
	ID3D11BlendState* m_blendAdditive;
	ID3D11BlendState* m_blendAlpha;

	float m_cloudRotation;
	uint32_t m_individualRotation;
	float m_cameraRoll;
	float m_wind;
	bool m_ready;
};
