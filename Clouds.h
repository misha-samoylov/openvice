#pragma once

#include <stdint.h>

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
	void Render(DXRender* render, Camera* camera, FXMVECTOR sunDirToward, bool drawClouds = true);
	void Cleanup();

private:
	struct CloudVertex {
		float x, y;
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

	static constexpr float SUN_SIZE = 2.5f;
	static constexpr float CLOUD_BOTTOM = 140.0f;
	static constexpr float CLOUD_TOP = 320.0f;
	static constexpr float CLOUD_COVERAGE = 0.48f;
	static constexpr float CLOUD_DENSITY = 0.055f;
	static constexpr float CLOUD_ABSORPTION = 0.55f;
	static constexpr float CLOUD_WIND = 12.0f;
	static constexpr float CLOUD_AMBIENT = 1.15f;

	bool CreatePipeline(DXRender* render);
	bool CreateTargets(DXRender* render);
	void ReleaseTargets(DXRender* render = nullptr);

	bool ProjectGtaPoint(Camera* camera, float gtaX, float gtaY, float gtaZ,
		float screenW, float screenH,
		float* outSX, float* outSY, float* outSzx, float* outSzy) const;

	void RenderSun(DXRender* render, Camera* camera, FXMVECTOR sunDirToward,
		float screenW, float screenH);
	void RenderVolumetric(DXRender* render, Camera* camera, FXMVECTOR sunDirToward);

	void FlushBatch(DXRender* render, ID3D12PipelineState* pso,
		CloudVertex* verts, int vertCount);
	void DrawFullscreen(DXRender* render, ID3D12PipelineState* pso);

	ID3D12RootSignature* m_rootSigSun;
	ID3D12RootSignature* m_rootSigCloud;
	ID3D12PipelineState* m_psoSun;
	ID3D12PipelineState* m_psoCloud;
	ID3D12PipelineState* m_psoComposite;

	UINT m_samplerIndex;

	ID3D12Resource* m_cloudTex;
	UINT m_cloudRtv;
	UINT m_cloudSrv;
	D3D12_RESOURCE_STATES m_cloudState;
	UINT m_fullW;
	UINT m_fullH;
	UINT m_halfW;
	UINT m_halfH;

	float m_time;
	float m_wind;
	bool m_ready;
};
