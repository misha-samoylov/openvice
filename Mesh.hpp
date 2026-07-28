#pragma once

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include <d3d11.h>
#include <DirectXMath.h>
#include <Dds.h>
#include <DirectXTex.h>

#pragma comment(lib, "d3d11.lib")

#include "DXRender.hpp"
#include "Camera.hpp"

using namespace DirectX;

struct DDS_File {
	DWORD dwMagic; // (ASCII "DDS ")
	struct DDS_HEADER header;
};

#define FOURCC_DXT1 (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT3 (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4 (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5 (MAKEFOURCC('D','X','T','5'))

struct objectConstBuffer
{
	XMMATRIX WVP;
	XMMATRIX World;
	XMMATRIX LightVP;
	XMFLOAT4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
	float windTime;
	float windAmount;
	float padWind[2];
};

enum MeshPass
{
	MESH_PASS_COLOR = 0,
	MESH_PASS_SHADOW = 1
};

/* Cached D3D bindings for the current frame — skip redundant Set* calls. */
struct MeshRenderContext
{
	XMMATRIX viewProj;
	XMMATRIX lightViewProj;
	XMFLOAT4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
	float windTime;
	MeshPass pass;
	ID3D11InputLayout* layout;
	ID3D11VertexShader* vs;
	ID3D11PixelShader* ps;
	ID3D11SamplerState* sampler;
	ID3D11SamplerState* shadowSampler;
	ID3D11Buffer* vb;
	ID3D11Buffer* ib;
	ID3D11ShaderResourceView* srv;
	ID3D11ShaderResourceView* shadowSRV;
	D3D_PRIMITIVE_TOPOLOGY topology;

	MeshRenderContext()
		: fogColor(0.49804f, 0.78431f, 0.94510f, 1.0f)
		, fogStart(600.0f)
		, fogEnd(1230.0f)
		, receiveShadows(0.0f)
		, shadowBias(0.0015f)
		, windTime(0.0f)
		, pass(MESH_PASS_COLOR)
		, layout(nullptr)
		, vs(nullptr)
		, ps(nullptr)
		, sampler(nullptr)
		, shadowSampler(nullptr)
		, vb(nullptr)
		, ib(nullptr)
		, srv(nullptr)
		, shadowSRV(nullptr)
		, topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		viewProj = XMMatrixIdentity();
		lightViewProj = XMMatrixIdentity();
	}

	void ClearBindings()
	{
		layout = nullptr;
		vs = nullptr;
		ps = nullptr;
		sampler = nullptr;
		shadowSampler = nullptr;
		vb = nullptr;
		ib = nullptr;
		srv = nullptr;
		shadowSRV = nullptr;
		topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
};

class Mesh
{
public:
	HRESULT Init(
		DXRender*pRender, 
		float *vertices, 
		int verticesCount, 
		unsigned int *indices, 
		int indicesCount, 
		D3D_PRIMITIVE_TOPOLOGY topology
	);
	void Cleanup();
	void Render(DXRender *pRender, MeshRenderContext& ctx);
	HRESULT SetDataDDS(
		DXRender *pRender,
		uint8_t *pDataSource,
		size_t size, 
		uint32_t width, 
		uint32_t height, 
		uint32_t dxtCompression, 
		uint32_t depth
	);
	void SetWorld(CXMMATRIX world) { m_World = world; }
	void SetPosition(
		float x, float y, float z,
		float scaleX, float scaleY, float scaleZ,
		float rotx, float roty, float rotz, float rotr
	);

	void SetId(int id) { m_meshId = id; }
	int GetId() { return m_meshId; }

	void SetAlpha(bool a) { m_hasAlpha = a; }
	bool GetAlpha() { return m_hasAlpha; }

	/* DXT1 1-bit alpha → cutout; DXT3/5 → soft blended. */
	void SetAlphaCutout(bool cutout) { m_alphaCutout = cutout; }
	bool IsAlphaCutout() const { return m_alphaCutout; }

	/* Palm/tree frond sway amplitude in local meters (0 = off). */
	void SetWindAmount(float amount) { m_windAmount = amount; }
	float GetWindAmount() const { return m_windAmount; }

	/* Interleaved xyz + uv (5 floats per vertex). Mutable for fake deformation. */
	std::vector<float>& GetVertexData() { return m_vertexData; }
	const std::vector<float>& GetVertexData() const { return m_vertexData; }
	int GetVertexCount() const { return (int)m_vertexData.size() / 5; }
	void UploadVertices(DXRender* pRender);

	static void ReleaseSharedResources();

private:
	HRESULT CreateConstBuffer(DXRender *pRender);
	static HRESULT EnsureSharedPipeline(DXRender *pRender);
	HRESULT CreateDataBuffer(
		DXRender *pRender,
		float *pVerticesData,
		int verticesCount,
		unsigned int *pIndicesData,
		int indicesCount
	);

	ID3D11Buffer *m_pVertexBuffer;
	ID3D11Buffer *m_pIndexBuffer;
	ID3D11Buffer *m_pObjectBuffer;

	struct objectConstBuffer m_objectConstBuffer;

	XMMATRIX m_World;
	std::vector<float> m_vertexData;

	unsigned int m_countIndices;
	D3D_PRIMITIVE_TOPOLOGY m_primitiveTopology;

	ID3D11ShaderResourceView* m_pTexture;

	int m_meshId;
	bool m_hasAlpha;
	bool m_alphaCutout;
	float m_windAmount;

	static ID3D11VertexShader* s_pVertexShader;
	static ID3D11PixelShader* s_pPixelShader;
	static ID3D11PixelShader* s_pShadowPixelShader;
	static ID3D11InputLayout* s_pVertexLayout;
	static ID3D11SamplerState* s_pSampler;
	static int s_sharedRefCount;
};
