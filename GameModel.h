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
	ID3D11VertexShader *m_pVertexShader;
	ID3D11PixelShader *m_pPixelShader;

	ID3D11InputLayout *m_pVertexLayout; // Описание формата вершин

	ID3D11Buffer *m_pVertexBuffer;
	ID3D11Buffer *m_pIndexBuffer;
	ID3D11Buffer* m_pPerObjectBuffer;

	cbPerObject mPerObj;

	ID3DBlob *m_pVSBlob;

	XMMATRIX mWVP;
	XMMATRIX mWorld;

	HRESULT CreateConstBuffer(GameRender *render);
	HRESULT CreatePixelShader(GameRender *render);
	HRESULT CreateVertexShader(GameRender *render);
	HRESULT CreateInputLayout(GameRender *render);
	HRESULT CreateBufferModel(GameRender *render);

public:
	HRESULT Init(GameRender *render);
	void Cleanup();
	void Render(GameRender * render, GameCamera *camera);
};

