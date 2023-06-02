#pragma once

#include <string>
#include <stdio.h>

#include <d3d11.h>
#include <DirectXMath.h>
#include <Dds.h>
#include <DirectXTex.h>

#pragma comment(lib, "d3d11.lib")

#include "Camera.hpp"
#include "DXRender.hpp"

using namespace DirectX;

struct DDS_File {
	DWORD dwMagic; // (ASCII "DDS ")
	struct DDS_HEADER header;
};

//#define DDSF_FOURCC 0x00000004l
#define FOURCC_DXT1 (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT3 (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4 (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5 (MAKEFOURCC('D','X','T','5'))

struct objectConstBuffer
{
	XMMATRIX WVP;
};

class Mesh
{
public:
	HRESULT Init(DXRender*pRender, float *vertices, int verticesCount, unsigned int *indices, int indicesCount, D3D_PRIMITIVE_TOPOLOGY topology);
	void Cleanup();
	void Render(DXRender*pRender, Camera *pCamera);
	HRESULT SetDataDDS(DXRender* pRender, uint8_t* pSource, size_t size, uint32_t width, uint32_t height, uint32_t dxtCompression, uint32_t depth);
	void SetPosition(float x, float y, float z,
		float scaleX, float scaleY, float scaleZ,
		float rotx, float roty, float rotz, float rotr);

	void SetName(std::string name) { m_name = name; }
	std::string GetName() { return m_name; }

	void SetId(int id) { m_id = id; }
	int GetId() { return m_id; }

private:
	HRESULT CreateConstBuffer(DXRender*pRender);
	HRESULT CreatePixelShader(DXRender*pRender);
	HRESULT CreateVertexShader(DXRender*pRender);
	HRESULT CreateInputLayout(DXRender*pRender);
	HRESULT CreateDataBuffer(DXRender*pRender,
		float *pVertices, int verticesCount,
		unsigned int *pIndices, int indicesCount);

	ID3D11VertexShader *m_pVertexShader;
	ID3D11PixelShader *m_pPixelShader;

	ID3D11InputLayout *m_pVertexLayout;

	ID3D11Buffer *m_pVertexBuffer;
	ID3D11Buffer *m_pIndexBuffer;
	ID3D11Buffer *m_pObjectBuffer;

	ID3DBlob *m_pVSBlob;
	struct objectConstBuffer m_objectConstBuffer;

	XMMATRIX m_WVP;
	XMMATRIX m_World;

	unsigned int m_countIndices;
	D3D_PRIMITIVE_TOPOLOGY m_primitiveTopology;

	ID3D11ShaderResourceView* m_pTexture;
	ID3D11SamplerState* m_pTextureSampler;
	void* tgaFileSource;
	size_t tgaFileSize;

	std::string m_name;
	int m_id;
};