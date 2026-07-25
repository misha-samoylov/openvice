#pragma once

#include <string>
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
};

/* Cached D3D bindings for the current frame — skip redundant Set* calls. */
struct MeshRenderContext
{
	XMMATRIX viewProj;
	ID3D11InputLayout* layout;
	ID3D11VertexShader* vs;
	ID3D11PixelShader* ps;
	ID3D11SamplerState* sampler;
	ID3D11Buffer* vb;
	ID3D11Buffer* ib;
	ID3D11ShaderResourceView* srv;
	D3D_PRIMITIVE_TOPOLOGY topology;

	MeshRenderContext()
		: layout(nullptr)
		, vs(nullptr)
		, ps(nullptr)
		, sampler(nullptr)
		, vb(nullptr)
		, ib(nullptr)
		, srv(nullptr)
		, topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
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

	unsigned int m_countIndices;
	D3D_PRIMITIVE_TOPOLOGY m_primitiveTopology;

	ID3D11ShaderResourceView* m_pTexture;

	int m_meshId;
	bool m_hasAlpha;
	bool m_alphaCutout;

	static ID3D11VertexShader* s_pVertexShader;
	static ID3D11PixelShader* s_pPixelShader;
	static ID3D11InputLayout* s_pVertexLayout;
	static ID3D11SamplerState* s_pSampler;
	static int s_sharedRefCount;
};
