#pragma once

#include <stdint.h>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"
#include "Frustum.h"

using namespace DirectX;

class Water
{
public:
	bool Init(DXRender* render, const char* waterproPath, const char* particleTxdPath);
	void Update(float deltaTime);
	void Render(DXRender* render, Camera* camera, Frustum& frustum, float drawDistance,
		FXMVECTOR sunDirToward, bool reflectClouds = true);
	void Cleanup();

private:
	struct WaterVertex {
		float x, y, z;
		float u, v;
	};

	struct WaterCB {
		XMMATRIX wvp;
		XMFLOAT2 uvScroll;
		float fogStart;
		float fogEnd;
		XMFLOAT4 tint;
		XMFLOAT4 fogColor;
		XMFLOAT3 cameraPos;
		float time;
		XMFLOAT3 sunDir;
		float cloudReflect;
		float windSpeed;
		float cloudCoverage;
		float cloudDensity;
		float pad1;
	};

	bool LoadWaterPro(const char* path);
	bool LoadWaterTexture(DXRender* render, const char* particleTxdPath);
	bool CreatePipeline(DXRender* render);
	bool BuildCoastMesh(DXRender* render);
	bool CreateOceanBuffers(DXRender* render);
	void AddQuad(std::vector<WaterVertex>& verts, std::vector<uint32_t>& indices,
		float gx, float gy, float gz, float size, float uvScale);
	void BindPipeline(DXRender* render, Camera* camera, float drawDistance,
		FXMVECTOR sunDirToward, bool reflectClouds);
	void DrawCoast(DXRender* render);
	void DrawInfiniteOcean(DXRender* render, Camera* camera, float drawDistance);
	bool IsInsideMapWaterGrid(float gtaX, float gtaY) const;
	void ComputeSeaLevel();

	static const int MAX_LEVELS = 48;
	static const int LARGE_SECTORS = 64;
	static const int SMALL_SECTORS = 128;
	static const int8_t NO_WATER = -128;
	static const int MAX_OCEAN_QUADS = 2048;

	int32_t m_numLevels;
	float m_waterZs[MAX_LEVELS];
	int8_t m_largeBlocks[LARGE_SECTORS][LARGE_SECTORS];
	int8_t m_fineBlocks[SMALL_SECTORS][SMALL_SECTORS];
	float m_seaZ;

	ID3D11Buffer* m_vb;
	ID3D11Buffer* m_ib;
	ID3D11Buffer* m_oceanVB;
	ID3D11Buffer* m_oceanIB;
	ID3D11Buffer* m_cb;
	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11InputLayout* m_layout;
	ID3D11ShaderResourceView* m_texture;
	ID3D11SamplerState* m_sampler;
	ID3D11RasterizerState* m_rasterizer;
	ID3D11DepthStencilState* m_depthState;
	ID3D11BlendState* m_blendState;

	uint32_t m_indexCount;
	float m_uvU;
	float m_uvV;
	float m_time;
	bool m_ready;
};
