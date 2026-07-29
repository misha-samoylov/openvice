#pragma once

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include <DirectXMath.h>

#include "DXRender.hpp"
#include "Camera.hpp"

#include <Dds.h>
#include <DirectXTex.h>

using namespace DirectX;

struct DDS_File {
	DWORD dwMagic;
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
	XMMATRIX LightVP[4];
	XMFLOAT4 cascadeSplits;
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

struct MeshRenderContext
{
	XMMATRIX viewProj;
	XMMATRIX lightViewProj[4];
	XMFLOAT4 cascadeSplits;
	XMFLOAT4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
	float windTime;
	MeshPass pass;
	ID3D12PipelineState* pso;
	ID3D12Resource* vb;
	ID3D12Resource* ib;
	UINT srvIndex;
	UINT shadowSrvIndex;
	UINT samplerIndex;
	UINT shadowSamplerIndex;
	D3D12_PRIMITIVE_TOPOLOGY topology;

	MeshRenderContext()
		: cascadeSplits(25.0f, 80.0f, 200.0f, 500.0f)
		, fogColor(0.49804f, 0.78431f, 0.94510f, 1.0f)
		, fogStart(600.0f)
		, fogEnd(1230.0f)
		, receiveShadows(0.0f)
		, shadowBias(0.00015f)
		, windTime(0.0f)
		, pass(MESH_PASS_COLOR)
		, pso(nullptr)
		, vb(nullptr)
		, ib(nullptr)
		, srvIndex(UINT_MAX)
		, shadowSrvIndex(UINT_MAX)
		, samplerIndex(UINT_MAX)
		, shadowSamplerIndex(UINT_MAX)
		, topology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		viewProj = XMMatrixIdentity();
		for (int i = 0; i < 4; i++)
			lightViewProj[i] = XMMatrixIdentity();
	}

	void ClearBindings()
	{
		pso = nullptr;
		vb = nullptr;
		ib = nullptr;
		srvIndex = UINT_MAX;
		shadowSrvIndex = UINT_MAX;
		samplerIndex = UINT_MAX;
		shadowSamplerIndex = UINT_MAX;
		topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
};

class Mesh
{
public:
	HRESULT Init(
		DXRender* pRender,
		float* vertices,
		int verticesCount,
		unsigned int* indices,
		int indicesCount,
		D3D_PRIMITIVE_TOPOLOGY topology
	);
	void Cleanup();
	void Render(DXRender* pRender, MeshRenderContext& ctx);
	HRESULT SetDataDDS(
		DXRender* pRender,
		uint8_t* pDataSource,
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

	void SetAlphaCutout(bool cutout) { m_alphaCutout = cutout; }
	bool IsAlphaCutout() const { return m_alphaCutout; }

	void SetWindAmount(float amount) { m_windAmount = amount; }
	float GetWindAmount() const { return m_windAmount; }

	std::vector<float>& GetVertexData() { return m_vertexData; }
	const std::vector<float>& GetVertexData() const { return m_vertexData; }
	int GetVertexCount() const { return (int)m_vertexData.size() / 5; }
	void UploadVertices(DXRender* pRender);

	static void ReleaseSharedResources();

private:
	static HRESULT EnsureSharedPipeline(DXRender* pRender);
	HRESULT CreateDataBuffer(
		DXRender* pRender,
		float* pVerticesData,
		int verticesCount,
		unsigned int* pIndicesData,
		int indicesCount
	);
	ID3D12PipelineState* SelectPso(DXRender* pRender, MeshRenderContext& ctx) const;

	ID3D12Resource* m_pVertexBuffer;
	ID3D12Resource* m_pIndexBuffer;
	GpuTexture m_texture;

	struct objectConstBuffer m_objectConstBuffer;

	XMMATRIX m_World;
	std::vector<float> m_vertexData;

	unsigned int m_countIndices;
	D3D_PRIMITIVE_TOPOLOGY m_primitiveTopology;

	int m_meshId;
	bool m_hasAlpha;
	bool m_alphaCutout;
	float m_windAmount;

	static ID3D12RootSignature* s_rootSig;
	static ID3D12PipelineState* s_psoOpaque;
	static ID3D12PipelineState* s_psoCutout;
	static ID3D12PipelineState* s_psoSoft;
	static ID3D12PipelineState* s_psoShadow;
	static ID3D12PipelineState* s_psoWire;
	static UINT s_samplerIndex;
	static UINT s_shadowSamplerIndex;
	static int s_sharedRefCount;
	static ID3DBlob* s_vsBlob;
	static ID3DBlob* s_psBlob;
	static ID3DBlob* s_shadowPsBlob;
};
