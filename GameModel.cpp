#include "GameModel.hpp"

//#include <DDSTextureLoader.h>
#include <DirectXTex.h>

using namespace DirectX;

HRESULT GameModel::CreateConstBuffer(GameRender *pRender)
{
	HRESULT hr;

	D3D11_BUFFER_DESC bdcb;
	ZeroMemory(&bdcb, sizeof(D3D11_BUFFER_DESC));

	bdcb.Usage = D3D11_USAGE_DEFAULT;
	bdcb.ByteWidth = sizeof(struct objectConstBuffer);
	bdcb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bdcb.CPUAccessFlags = 0;
	bdcb.MiscFlags = 0;

	hr = pRender->GetDevice()->CreateBuffer(&bdcb, NULL, &m_pObjectBuffer);

	return hr;
}

void GameModel::Cleanup()
{
	/* clear buffers */
	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();
	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();
	if (m_pObjectBuffer)
		m_pObjectBuffer->Release();

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
	pRender->GetDeviceContext()->IASetInputLayout(m_pVertexLayout);

	/* set index buffer */
	pRender->GetDeviceContext()->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	/* set vertex buffer */
	UINT stride = sizeof(float) * 5; /* x, y, z, tx, ty */ 
	UINT offset = 0;
	pRender->GetDeviceContext()->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	/* set topology */
	pRender->GetDeviceContext()->IASetPrimitiveTopology(this->m_primitiveTopology);

	/* set shaders */
	pRender->GetDeviceContext()->VSSetShader(m_pVertexShader, NULL, 0);
	pRender->GetDeviceContext()->PSSetShader(m_pPixelShader, NULL, 0);

	/* calculate position */
	m_WVP = m_World * pCamera->GetView() * pCamera->GetProjection();
	m_objectConstBuffer.WVP = XMMatrixTranspose(m_WVP);

	/* send position to shader */
	pRender->GetDeviceContext()->UpdateSubresource(m_pObjectBuffer, 0, NULL, &m_objectConstBuffer, 0, 0);
	pRender->GetDeviceContext()->VSSetConstantBuffers(0, 1, &m_pObjectBuffer);


	///////////////**************new**************////////////////////
	pRender->GetDeviceContext()->PSSetShaderResources(0, 1, &CubesTexture);
	pRender->GetDeviceContext()->PSSetSamplers(0, 1, &CubesTexSamplerState);
	///////////////**************new**************////////////////////

	/* render indexed vertices */
	pRender->GetDeviceContext()->DrawIndexed(m_countIndices, 0, 0);
}

void GameModel::InitPosition()
{
	XMVECTOR vector = XMVectorSet(-1.0, 0.0, 0.0, 0.0);
	XMMATRIX modelRotation = XMMatrixRotationAxis(vector, 90.0f);

	XMMATRIX modelPosition = XMMatrixIdentity();
	XMMATRIX modelScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX modelTranslation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	m_World = modelRotation * modelPosition * modelScale * modelTranslation;
}

void GameModel::setTgaFile(GameRender* pRender, void* source, size_t size)
{
	HRESULT hr;
		tgaFileSource = source; tgaFileSize = size;



		///////////////**************new**************////////////////////
		//hr = CreateDDSTextureFromFile(pRender->GetDevice(), L"C:/Users/john/Documents/GitHub/openvice/test-dxt5.dds", nullptr, &CubesTexture);
		//if (FAILED(hr)) {
		//	printf("Error: cannot create dds file\n");
		//}

		//auto image = std::make_unique<ScratchImage>();
		ScratchImage image;
		hr = LoadFromTGAFile(L"C:\\Users\\master\\Downloads\\lawyer1.tga", TGA_FLAGS_NONE, nullptr, image);
		//hr = LoadFromTGAMemory(tgaFileSource, tgaFileSize, TGA_FLAGS_NONE, nullptr, image);
		if (FAILED(hr)) {
			printf("Error: cannot load tga file\n");
		}

		//	ScratchImage destImage;
		//hr = FlipRotate(image.GetImages(), image.GetImageCount(),
		//	image.GetMetadata(),
		//	TEX_FR_ROTATE270, destImage);
		// error

	//ID3D11ShaderResourceView* pSRV = nullptr;
		hr = CreateShaderResourceView(pRender->GetDevice(),
			image.GetImages(), image.GetImageCount(),
			image.GetMetadata(), &CubesTexture);
		if (FAILED(hr)) {
			printf("Error: cannot CreateShaderResourceView dds file\n");
		}

		// Describe the Sample State
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		//sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		//sampDesc.MaxAnisotropy = 16;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		//sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		//Create the Sample State
		hr = pRender->GetDevice()->CreateSamplerState(&sampDesc, &CubesTexSamplerState);
		///////////////**************new**************////////////////////


}

HRESULT GameModel::CreatePixelShader(GameRender *pRender)
{
	HRESULT hr;

	/* compile shader from file */
	/* ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob); */

	ID3DBlob *pPSBlob = NULL;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);

	if (FAILED(hr)) {
		printf("Error: cannot read compiled pixel shader\n");
		return hr;
	}

	hr = pRender->GetDevice()->CreatePixelShader(pPSBlob->GetBufferPointer(),
		pPSBlob->GetBufferSize(), NULL, &m_pPixelShader);

	pPSBlob->Release();

	if (FAILED(hr)) {
		printf("Error: cannot create pixel shader\n");
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

	hr = pRender->GetDevice()->CreateVertexShader(m_pVSBlob->GetBufferPointer(),
		m_pVSBlob->GetBufferSize(), NULL, &m_pVertexShader);

	if (FAILED(hr)) {
		printf("Error: cannot create vertex shader\n");
		m_pVSBlob->Release();
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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	hr = pRender->GetDevice()->CreateInputLayout(layout, numElements,
		m_pVSBlob->GetBufferPointer(), m_pVSBlob->GetBufferSize(), &m_pVertexLayout);

	if (FAILED(hr)) {
		printf("Error: cannot create input layout\n");
	}

	return hr;
}

HRESULT GameModel::CreateDataBuffer(GameRender * pRender,
	float *vertices, int verticesCount,	unsigned int *indices, int indicesCount)
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

	hr = pRender->GetDevice()->CreateBuffer(&bdv, &datav, &m_pVertexBuffer);

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

	hr = pRender->GetDevice()->CreateBuffer(&bdi, &datai, &m_pIndexBuffer);

	if (FAILED(hr)) {
		printf("Error: cannot create index data buffer\n");
	}




	return hr;
}

HRESULT GameModel::Init(GameRender *pRender, float *pVertices, int verticesCount,
	unsigned int *pIndices, int indicesCount, 
	D3D_PRIMITIVE_TOPOLOGY topology)
{
	HRESULT hr;

	m_pVSBlob = NULL;

	m_countIndices = indicesCount;
	m_primitiveTopology = topology;

	InitPosition();

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
	hr = CreateDataBuffer(pRender, pVertices, verticesCount,
		pIndices, indicesCount);
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