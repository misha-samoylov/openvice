#include "GameModel.hpp"

#include <DDSTextureLoader.h>
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
	pRender->GetDeviceContext()->PSSetShaderResources(0, 1, &m_pTexture);
	pRender->GetDeviceContext()->PSSetSamplers(0, 1, &m_pTextureSampler);
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


HRESULT GameModel::SetDataDDS(GameRender* pRender, uint8_t* source, size_t size, uint32_t width, uint32_t height, uint32_t dxtCompression)
{
	HRESULT hr;

	//tgaFileSource = source;
	//tgaFileSize = size;

	///////////////**************new**************////////////////////
	//hr = CreateDDSTextureFromFile(pRender->GetDevice(), L"C:/Users/john/Documents/GitHub/openvice/test-dxt5.dds", nullptr, &CubesTexture);
	//if (FAILED(hr)) {
	//	printf("Error: cannot create dds file\n");
	//}

	struct DDS_File dds;
	dds.dwMagic = DDS_MAGIC;
	dds.header.size = sizeof(struct DDS_HEADER);
	dds.header.flags = 0; // 0
	dds.header.width = width;
	dds.header.height = height;
	dds.header.pitchOrLinearSize = width * height;
	dds.header.mipMapCount = 0;
	dds.header.ddspf.size = sizeof(struct DDS_PIXELFORMAT);
	//ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
	dds.header.ddspf.flags = DDS_FOURCC; // DDS_PAL8;
	switch (dxtCompression) {
	case 1:
		dds.header.ddspf.fourCC = FOURCC_DXT1;
		break;
	case 3:
		dds.header.ddspf.fourCC = FOURCC_DXT3;
		break;
	case 4:
		dds.header.ddspf.fourCC = FOURCC_DXT4;
		break;
	default:
		dds.header.ddspf.fourCC = FOURCC_DXT1;
		break;
	}
	// ddsd.ddpfPixelFormat.dwFourCC = bpp == 24 ? FOURCC_DXT1 : FOURCC_DXT5;

	size_t len = sizeof(dds) + size;
	uint8_t* buf = (uint8_t*)malloc(len);
	memcpy(buf, &dds, sizeof(dds));
	memcpy(buf + 1*sizeof(dds), source , size);

	//FILE* f = fopen("test.dds", "wb");
	//fwrite(buf, len, 1, f);
	//fclose(f);

	//auto image = std::make_unique<ScratchImage>();
	ScratchImage image;
	//hr = LoadFromTGAFile(L"C:\\Users\\master\\Downloads\\lawyer1.tga", TGA_FLAGS_NONE, nullptr, image);
	hr = LoadFromDDSMemory(buf, len, DDS_FLAGS_NONE, nullptr, image);
	//hr = LoadFromDDSFile(L"test.dds", DDS_FLAGS_FORCE_DX9_LEGACY, nullptr, image);
	if (FAILED(hr)) {
		printf("Error: cannot load tga file\n");
	}

	//ID3D11ShaderResourceView* pSRV = nullptr;
	hr = CreateShaderResourceView(pRender->GetDevice(),
		image.GetImages(), image.GetImageCount(),
		image.GetMetadata(), &m_pTexture);

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

	hr = pRender->GetDevice()->CreateSamplerState(&sampDesc, &m_pTextureSampler);

	return hr;
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
	// Указываем форму данных в вершинном шейдере
	HRESULT hr;
	
	// Координаты вершин X Y Z
	D3D11_INPUT_ELEMENT_DESC layout[2];
	layout[0].SemanticName = "POSITION";
	layout[0].SemanticIndex = 0;
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // X Y Z
	layout[0].InputSlot = 0;
	layout[0].AlignedByteOffset = 0;
	layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[0].InstanceDataStepRate = 0;

	// Координаты текстуры X Y
	layout[1].SemanticName = "TEXCOORD";
	layout[1].SemanticIndex = 0;
	layout[1].Format = DXGI_FORMAT_R32G32_FLOAT; // X Y
	layout[1].InputSlot = 0;
	layout[1].AlignedByteOffset = 12; // Смещение от вершин X Y Z
	layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[1].InstanceDataStepRate = 0;
	
	UINT numElements = ARRAYSIZE(layout);

	hr = pRender->GetDevice()->CreateInputLayout(layout, numElements,
		m_pVSBlob->GetBufferPointer(), m_pVSBlob->GetBufferSize(), &m_pVertexLayout);

	if (FAILED(hr)) {
		printf("Error: cannot CreateInputLayout\n");
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
		printf("Error: cannot CreateBuffer vertex buffer\n");
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

	if (FAILED(hr))
		printf("Error: cannot CreateBuffer index buffer\n");

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
