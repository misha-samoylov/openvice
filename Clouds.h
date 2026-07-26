#pragma once

#include <stdint.h>

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

/*
 * re3 miami CClouds — camera-locked billboard sprites from PARTICLE.TXD.
 * Drawn after sky clear, before world (depth off).
 */
class Clouds
{
public:
	bool Init(DXRender* render, const char* particleTxdPath);
	void Update(float dt, Camera* camera);
	void Render(DXRender* render, Camera* camera);
	void Cleanup();

private:
	struct CloudVertex {
		float x, y; /* NDC */
		float u, v;
		float r, g, b, a;
	};

	enum { TEX_CLOUD1 = 0, TEX_CLOUD2, TEX_CLOUD3, TEX_HILIT, TEX_MASKED, TEX_COUNT };
	enum { MAX_QUADS = 64 };

	bool LoadTextures(DXRender* render, const char* particleTxdPath);
	bool CreatePipeline(DXRender* render);
	bool CreateStates(DXRender* render);

	bool ProjectGtaPoint(Camera* camera, float gtaX, float gtaY, float gtaZ,
		float screenW, float screenH,
		float* outSX, float* outSY, float* outSzx, float* outSzy) const;

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
