#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "GameCamera.h"
#include "GameRender.h"

using namespace DirectX; /* DirectXMath */

struct SimpleVertex
{
	float x, y, z;
};

struct cbPerObject
{
	XMMATRIX WVP;
};

class GameModel
{
private:
	ID3D11VertexShader *g_pVertexShader;
	ID3D11PixelShader *g_pPixelShader;

	ID3D11InputLayout *g_pVertexLayout; // Описание формата вершин

	ID3D11Buffer *g_pVertexBuffer;
	ID3D11Buffer *g_pIndexBuffer;
	ID3D11Buffer* cbPerObjectBuffer;

	cbPerObject cbPerObj;

	ID3DBlob *pVSBlob;

	XMMATRIX WVP;
	XMMATRIX World;

public:
	HRESULT Init(GameRender *render);
	void Cleanup();
	void Render(GameRender * render, GameCamera *camera);

	HRESULT CreateConstBuffer(GameRender *render);
	HRESULT CreatePixelShader(GameRender *render);
	HRESULT CreateVertexShader(GameRender *render);
	HRESULT CreateInputLayout(GameRender *render);
	HRESULT CreateBufferModel(GameRender *render);
};

