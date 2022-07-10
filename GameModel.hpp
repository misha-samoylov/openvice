#pragma once

#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "GameCamera.hpp"
#include "GameRender.hpp"

using namespace DirectX; /* DirectXMath */

struct objectConstBuffer
{
	XMMATRIX WVP;
};

class GameModel
{
public:
	HRESULT Init(GameRender *pRender, float *vertices, int verticesCount, 
		unsigned int *indices, int indicesCount,
		D3D_PRIMITIVE_TOPOLOGY topology);
	void Cleanup();
	void Render(GameRender *pRender, GameCamera *pCamera);

private:
	void InitPosition();
	HRESULT CreateConstBuffer(GameRender *pRender);
	HRESULT CreatePixelShader(GameRender *pRender);
	HRESULT CreateVertexShader(GameRender *pRender);
	HRESULT CreateInputLayout(GameRender *pRender);
	HRESULT CreateDataBuffer(GameRender *pRender,
		float *pVertices, int verticesCount,
		unsigned int *pIndices, int indicesCount);

	HRESULT CompileShaderFromFile(LPCWSTR szFileName, 
		LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

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
};