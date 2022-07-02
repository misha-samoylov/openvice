#include "GameModel.hpp"

HRESULT GameModel::CreateConstBuffer(GameRender *pRender)
{
	HRESULT hr;

	D3D11_BUFFER_DESC cbbd;
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerObject);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;

	hr = pRender->getDevice()->CreateBuffer(&cbbd, NULL, &m_pPerObjectBuffer);
	return hr;
}

void GameModel::Cleanup()
{
	/* clear buffers */
	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();
	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();
	if (m_pPerObjectBuffer)
		m_pPerObjectBuffer->Release();

	/* clear layout */
	if (m_pVertexLayout)
		m_pVertexLayout->Release();

	/* clear shaders */
	if (m_pVertexShader) 
		m_pVertexShader->Release();
	if (m_pPixelShader) 
		m_pPixelShader->Release();
}

void GameModel::Render(GameRender * pRender, GameCamera *pCamera)
{
	pRender->getDeviceContext()->IASetInputLayout(m_pVertexLayout);

	/* set index buffer */
	pRender->getDeviceContext()->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	/* set vertex buffer */
	UINT stride = sizeof(float) * 3;
	UINT offset = 0;
	pRender->getDeviceContext()->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	pRender->getDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	/* set shaders */
	pRender->getDeviceContext()->VSSetShader(m_pVertexShader, NULL, 0);
	pRender->getDeviceContext()->PSSetShader(m_pPixelShader, NULL, 0);

	XMMATRIX modelPosition = XMMatrixIdentity();
	XMMATRIX modelScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX modelTranslation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	mWorld = modelPosition * modelScale * modelTranslation;

	mWVP = mWorld * pCamera->getView() * pCamera->getProjection();
	mPerObj.WVP = XMMatrixTranspose(mWVP);

	pRender->getDeviceContext()->UpdateSubresource(m_pPerObjectBuffer, 0, NULL, &mPerObj, 0, 0);
	pRender->getDeviceContext()->VSSetConstantBuffers(0, 1, &m_pPerObjectBuffer);

	/* render indexed vertices */
	pRender->getDeviceContext()->DrawIndexed(6, 0, 0);
}


HRESULT GameModel::CreatePixelShader(GameRender *pRender)
{
	HRESULT hr = S_OK;

	/* compile shader from file */
	/* ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob); */

	ID3DBlob* pPSBlob = NULL;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);

	if (FAILED(hr)) {
		printf("Error: cannot read compiled pixel shader\n");
		return hr;
	}

	hr = pRender->getDevice()->CreatePixelShader(pPSBlob->GetBufferPointer(),
		pPSBlob->GetBufferSize(), NULL, &m_pPixelShader);

	pPSBlob->Release();

	if (FAILED(hr)) {
		printf("Error: cannot create pixel shader\n");
		return hr;
	}

	return hr;
}

HRESULT GameModel::CreateVertexShader(GameRender *pRender)
{
	HRESULT hr;

	hr = D3DReadFileToBlob(L"vertex_shader.cso", &m_pVSBlob);

	if (FAILED(hr)) {
		printf("Error: cannot read compiled vertex shader\n");
		return hr;
	}

	hr = pRender->getDevice()->CreateVertexShader(m_pVSBlob->GetBufferPointer(),
		m_pVSBlob->GetBufferSize(), NULL, &m_pVertexShader);

	if (FAILED(hr)) {
		printf("Error: cannot create vertex shader\n");
		m_pVSBlob->Release();
		return hr;
	}

	return hr;
}

HRESULT GameModel::CreateInputLayout(GameRender *pRender)
{
	HRESULT hr;

	/* setup layout */
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{
			"POSITION",  /* name */
			0, /* index */
			DXGI_FORMAT_R32G32B32_FLOAT, /* size */
			0, /* incoming slot (0-15) */ 
			0, /* address start data in vertex buffer */
			D3D11_INPUT_PER_VERTEX_DATA, /* class incoming slot (no matter) */
			0 /* InstanceDataStepRate (no matter) */
		},
	};

	UINT numElements = ARRAYSIZE(layout);

	hr = pRender->getDevice()->CreateInputLayout(layout, numElements, m_pVSBlob->GetBufferPointer(),
		m_pVSBlob->GetBufferSize(), &m_pVertexLayout);

	if (FAILED(hr)) {
		printf("Error: cannot create input layout\n");
	}

	return hr;
}

HRESULT GameModel::CreateDataBuffer(GameRender * pRender, float *vertices, int verticesCount,
	unsigned int *indices, int indicesCount)
{
	HRESULT hr;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(float) * 12; /* size buffer */
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; /* type buffer = vertex buffer */
	bd.CPUAccessFlags = 0;

	{
		D3D11_SUBRESOURCE_DATA InitData; // buffer data
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = vertices; // pointer to data

		hr = pRender->getDevice()->CreateBuffer(&bd, &InitData, &m_pVertexBuffer);

		if (FAILED(hr)) {
			printf("Error: cannot create vertex data buffer\n");
			return hr;
		}
	}

	/* fill in a buffer description */
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(unsigned int) * 6;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	/* define the resource data */
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = indices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	hr = pRender->getDevice()->CreateBuffer(&bufferDesc, &InitData, &m_pIndexBuffer);
	if (FAILED(hr)) {
		printf("Error: cannot create index data buffer\n");
	}

	return hr;
}

HRESULT GameModel::Init(GameRender *pRender, float *vertices, int verticesCount,
	unsigned int *indices, int indicesCount)
{
	HRESULT hr = S_OK;

	m_pVSBlob = NULL;

	mWVP = XMMatrixIdentity();
	mWorld = XMMatrixIdentity();

	CreateVertexShader(pRender);
	CreatePixelShader(pRender);
	CreateConstBuffer(pRender);
	CreateInputLayout(pRender);
	CreateDataBuffer(pRender, vertices, verticesCount,
		indices, indicesCount);

	m_pVSBlob->Release();

	return hr;
}

HRESULT GameModel::CompileShaderFromFile(LPCWSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

	hr = D3DCompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, NULL);

	if (FAILED(hr)) {
		printf("Error: cannot compile shader\n");
	}

	return hr;
}