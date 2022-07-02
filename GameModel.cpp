#include "GameModel.hpp"

HRESULT GameModel::CreateConstBuffer(GameRender *pRender)
{
	HRESULT hr;

	D3D11_BUFFER_DESC bdcb;
	ZeroMemory(&bdcb, sizeof(D3D11_BUFFER_DESC));

	bdcb.Usage = D3D11_USAGE_DEFAULT;
	bdcb.ByteWidth = sizeof(cbPerObject);
	bdcb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bdcb.CPUAccessFlags = 0;
	bdcb.MiscFlags = 0;

	hr = pRender->getDevice()->CreateBuffer(&bdcb, NULL, &m_pPerObjectBuffer);

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
	UINT stride = sizeof(float) * 3; /* x, y, z */
	UINT offset = 0;
	pRender->getDeviceContext()->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	/* set topology */
	pRender->getDeviceContext()->IASetPrimitiveTopology(this->m_primitiveTopology);

	/* set shaders */
	pRender->getDeviceContext()->VSSetShader(m_pVertexShader, NULL, 0);
	pRender->getDeviceContext()->PSSetShader(m_pPixelShader, NULL, 0);

	/* set position */
	XMMATRIX modelPosition = XMMatrixIdentity();
	XMMATRIX modelScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX modelTranslation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	XMVECTOR vector = XMVectorSet(-1.0, 0.0, 0.0, 0.0);
	XMMATRIX modelRotation = XMMatrixRotationAxis(vector, 90.0f);

	m_World = modelRotation * modelPosition * modelScale * modelTranslation;

	m_WVP = m_World * pCamera->getView() * pCamera->getProjection();
	m_perObj.WVP = XMMatrixTranspose(m_WVP);

	pRender->getDeviceContext()->UpdateSubresource(m_pPerObjectBuffer, 0, NULL, &m_perObj, 0, 0);
	pRender->getDeviceContext()->VSSetConstantBuffers(0, 1, &m_pPerObjectBuffer);

	/* render indexed vertices */
	pRender->getDeviceContext()->DrawIndexed(m_countIndices, 0, 0);
}


HRESULT GameModel::CreatePixelShader(GameRender *pRender)
{
	HRESULT hr;

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

	/* vertices: fill in a buffer description */
	D3D11_BUFFER_DESC bdv;
	ZeroMemory(&bdv, sizeof(bdv));
	bdv.Usage = D3D11_USAGE_DEFAULT;
	bdv.ByteWidth = sizeof(float) * verticesCount; /* size buffer */
	bdv.BindFlags = D3D11_BIND_VERTEX_BUFFER; /* type buffer = vertex buffer */

	/* vertices: define the resource data */
	D3D11_SUBRESOURCE_DATA datav; /* buffer data */
	ZeroMemory(&datav, sizeof(datav));
	datav.pSysMem = vertices; /* pointer to data */

	hr = pRender->getDevice()->CreateBuffer(&bdv, &datav, &m_pVertexBuffer);

	if (FAILED(hr)) {
		printf("Error: cannot create vertex data buffer\n");
		return hr;
	}
	
	/* indices: fill in a buffer description */
	D3D11_BUFFER_DESC bdi;
	ZeroMemory(&bdi, sizeof(bdi));
	bdi.Usage = D3D11_USAGE_DEFAULT;
	bdi.ByteWidth = sizeof(unsigned int) * indicesCount;
	bdi.BindFlags = D3D11_BIND_INDEX_BUFFER;

	/* indices: define the resource data */
	D3D11_SUBRESOURCE_DATA datai;
	ZeroMemory(&datai, sizeof(datai));
	datai.pSysMem = indices;

	hr = pRender->getDevice()->CreateBuffer(&bdi, &datai, &m_pIndexBuffer);

	if (FAILED(hr)) {
		printf("Error: cannot create index data buffer\n");
		return hr;
	}

	return hr;
}

HRESULT GameModel::Init(GameRender *pRender, float *vertices, int verticesCount,
	unsigned int *indices, int indicesCount, D3D_PRIMITIVE_TOPOLOGY topology)
{
	HRESULT hr;

	m_pVSBlob = NULL;

	m_WVP = XMMatrixIdentity();
	m_World = XMMatrixIdentity();
	m_countIndices = indicesCount;
	m_primitiveTopology = topology;

	hr = CreateVertexShader(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create vertex shader\n");
		return hr;
	}
	hr = CreatePixelShader(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create pixel shader\n");
		return hr;
	}
	hr = CreateConstBuffer(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create const shader\n");
		return hr;
	}
	hr = CreateInputLayout(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create input layout\n");
		return hr;
	}
	hr = CreateDataBuffer(pRender, vertices, verticesCount,
		indices, indicesCount);
	if (FAILED(hr)) {
		printf("Error: cannot create data buffer\n");
		return hr;
	}

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