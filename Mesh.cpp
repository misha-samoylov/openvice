#include "Mesh.hpp"

ID3D11VertexShader* Mesh::s_pVertexShader = nullptr;
ID3D11PixelShader* Mesh::s_pPixelShader = nullptr;
ID3D11PixelShader* Mesh::s_pShadowPixelShader = nullptr;
ID3D11InputLayout* Mesh::s_pVertexLayout = nullptr;
ID3D11SamplerState* Mesh::s_pSampler = nullptr;
int Mesh::s_sharedRefCount = 0;

HRESULT Mesh::CreateConstBuffer(DXRender *pRender)
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

HRESULT Mesh::EnsureSharedPipeline(DXRender *pRender)
{
	HRESULT hr = S_OK;

	if (s_pVertexShader != nullptr)
		return S_OK;

	ID3DBlob *pVSBlob = nullptr;
	hr = D3DReadFileToBlob(L"vertex_shader.cso", &pVSBlob);
	if (FAILED(hr)) {
		printf("Error: cannot read compiled vertex shader\n");
		return hr;
	}

	hr = pRender->GetDevice()->CreateVertexShader(
		pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(),
		NULL,
		&s_pVertexShader
	);
	if (FAILED(hr)) {
		printf("Error: cannot create vertex shader\n");
		pVSBlob->Release();
		return hr;
	}

	D3D11_INPUT_ELEMENT_DESC layout[2];
	layout[0].SemanticName = "POSITION";
	layout[0].SemanticIndex = 0;
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[0].InputSlot = 0;
	layout[0].AlignedByteOffset = 0;
	layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[0].InstanceDataStepRate = 0;

	layout[1].SemanticName = "TEXCOORD";
	layout[1].SemanticIndex = 0;
	layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	layout[1].InputSlot = 0;
	layout[1].AlignedByteOffset = 12;
	layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[1].InstanceDataStepRate = 0;

	hr = pRender->GetDevice()->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(),
		&s_pVertexLayout
	);
	pVSBlob->Release();
	if (FAILED(hr)) {
		printf("Error: cannot CreateInputLayout\n");
		return hr;
	}

	ID3DBlob *pPSBlob = nullptr;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);
	if (FAILED(hr)) {
		printf("Error: cannot read compiled pixel shader\n");
		return hr;
	}

	hr = pRender->GetDevice()->CreatePixelShader(
		pPSBlob->GetBufferPointer(),
		pPSBlob->GetBufferSize(),
		NULL,
		&s_pPixelShader
	);
	pPSBlob->Release();
	if (FAILED(hr)) {
		printf("Error: cannot create pixel shader\n");
		return hr;
	}

	ID3DBlob *pShadowPSBlob = nullptr;
	hr = D3DReadFileToBlob(L"shadow_ps.cso", &pShadowPSBlob);
	if (FAILED(hr)) {
		printf("Error: cannot read compiled shadow pixel shader\n");
		return hr;
	}
	hr = pRender->GetDevice()->CreatePixelShader(
		pShadowPSBlob->GetBufferPointer(),
		pShadowPSBlob->GetBufferSize(),
		NULL,
		&s_pShadowPixelShader
	);
	pShadowPSBlob->Release();
	if (FAILED(hr)) {
		printf("Error: cannot create shadow pixel shader\n");
		return hr;
	}

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = pRender->GetDevice()->CreateSamplerState(&sampDesc, &s_pSampler);
	if (FAILED(hr)) {
		printf("Error: cannot create sampler state\n");
	}

	return hr;
}

void Mesh::ReleaseSharedResources()
{
	if (s_pSampler) {
		s_pSampler->Release();
		s_pSampler = nullptr;
	}
	if (s_pVertexLayout) {
		s_pVertexLayout->Release();
		s_pVertexLayout = nullptr;
	}
	if (s_pVertexShader) {
		s_pVertexShader->Release();
		s_pVertexShader = nullptr;
	}
	if (s_pPixelShader) {
		s_pPixelShader->Release();
		s_pPixelShader = nullptr;
	}
	if (s_pShadowPixelShader) {
		s_pShadowPixelShader->Release();
		s_pShadowPixelShader = nullptr;
	}
	s_sharedRefCount = 0;
}

void Mesh::Cleanup()
{
	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();
	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();
	if (m_pObjectBuffer)
		m_pObjectBuffer->Release();
	if (m_pTexture)
		m_pTexture->Release();

	m_pVertexBuffer = nullptr;
	m_pIndexBuffer = nullptr;
	m_pObjectBuffer = nullptr;
	m_pTexture = nullptr;

	if (s_sharedRefCount > 0) {
		s_sharedRefCount--;
		if (s_sharedRefCount == 0)
			ReleaseSharedResources();
	}
}

void Mesh::Render(DXRender* pRender, MeshRenderContext& ctx)
{
	ID3D11DeviceContext* ctx3d = pRender->GetDeviceContext();

	if (ctx.layout != s_pVertexLayout) {
		ctx3d->IASetInputLayout(s_pVertexLayout);
		ctx.layout = s_pVertexLayout;
	}

	if (ctx.ib != m_pIndexBuffer) {
		ctx3d->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		ctx.ib = m_pIndexBuffer;
	}

	if (ctx.vb != m_pVertexBuffer) {
		UINT stride = sizeof(float) * 5;
		UINT offset = 0;
		ctx3d->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
		ctx.vb = m_pVertexBuffer;
	}

	if (ctx.topology != m_primitiveTopology) {
		ctx3d->IASetPrimitiveTopology(m_primitiveTopology);
		ctx.topology = m_primitiveTopology;
	}

	if (ctx.vs != s_pVertexShader) {
		ctx3d->VSSetShader(s_pVertexShader, NULL, 0);
		ctx.vs = s_pVertexShader;
	}

	ID3D11PixelShader* desiredPS =
		(ctx.pass == MESH_PASS_SHADOW) ? s_pShadowPixelShader : s_pPixelShader;
	if (ctx.ps != desiredPS) {
		ctx3d->PSSetShader(desiredPS, NULL, 0);
		ctx.ps = desiredPS;
	}

	XMMATRIX wvp = XMMatrixMultiply(m_World, ctx.viewProj);
	m_objectConstBuffer.WVP = XMMatrixTranspose(wvp);
	m_objectConstBuffer.World = XMMatrixTranspose(m_World);
	m_objectConstBuffer.LightVP = XMMatrixTranspose(ctx.lightViewProj);
	m_objectConstBuffer.fogColor = ctx.fogColor;
	m_objectConstBuffer.fogStart = ctx.fogStart;
	m_objectConstBuffer.fogEnd = ctx.fogEnd;
	m_objectConstBuffer.receiveShadows =
		(ctx.pass == MESH_PASS_COLOR) ? ctx.receiveShadows : 0.0f;
	m_objectConstBuffer.shadowBias = ctx.shadowBias;
	ctx3d->UpdateSubresource(m_pObjectBuffer, 0, NULL, &m_objectConstBuffer, 0, 0);
	ctx3d->VSSetConstantBuffers(0, 1, &m_pObjectBuffer);
	ctx3d->PSSetConstantBuffers(0, 1, &m_pObjectBuffer);

	if (ctx.srv != m_pTexture) {
		ctx3d->PSSetShaderResources(0, 1, &m_pTexture);
		ctx.srv = m_pTexture;
	}

	if (ctx.pass == MESH_PASS_COLOR && ctx.shadowSRV) {
		if (ctx.shadowSRV) {
			ctx3d->PSSetShaderResources(1, 1, &ctx.shadowSRV);
		}
		if (ctx.shadowSampler && ctx.shadowSampler != nullptr) {
			ctx3d->PSSetSamplers(1, 1, &ctx.shadowSampler);
		}
	}

	if (ctx.sampler != s_pSampler) {
		ctx3d->PSSetSamplers(0, 1, &s_pSampler);
		ctx.sampler = s_pSampler;
	}

	ctx3d->DrawIndexed(m_countIndices, 0, 0);
}

void Mesh::SetPosition(float x, float y, float z,
	float scaleX, float scaleY, float scaleZ,
	float rotx, float roty, float rotz, float rotr)
{
	XMVECTOR vector = XMVectorSet(rotx, roty, rotz, rotr);
	XMMATRIX modelRotation = XMMatrixRotationQuaternion(vector);
	XMMATRIX modelScale = XMMatrixScaling(scaleX, scaleY, scaleZ);
	XMMATRIX modelTranslation = XMMatrixTranslation(x, y, z);

	m_World = modelRotation * modelScale * modelTranslation;
}

HRESULT Mesh::SetDataDDS(DXRender* pRender, uint8_t* pDataSourceDDS, size_t fileSizeDDS, uint32_t width, uint32_t height, uint32_t dxtCompression, uint32_t depth)
{
	HRESULT hr;

	struct DDS_File dds;
	dds.dwMagic = DDS_MAGIC;
	dds.header.size = sizeof(struct DDS_HEADER);
	dds.header.flags = 0;
	dds.header.width = width;
	dds.header.height = height;
	dds.header.pitchOrLinearSize = width * height;
	dds.header.mipMapCount = 0;
	dds.header.ddspf.size = sizeof(struct DDS_PIXELFORMAT);
	dds.header.ddspf.flags = DDS_FOURCC;
	switch (dxtCompression) {
	default:
	case 1:
		dds.header.ddspf.fourCC = FOURCC_DXT1;
		break;
	case 3:
		dds.header.ddspf.fourCC = FOURCC_DXT3;
		break;
	case 4:
		dds.header.ddspf.fourCC = FOURCC_DXT4;
		break;
	}

	size_t len = sizeof(dds) + fileSizeDDS;
	uint8_t* buf = (uint8_t*)malloc(len);
	memcpy(buf, &dds, sizeof(dds));
	memcpy(buf + sizeof(dds), pDataSourceDDS, fileSizeDDS);

	ScratchImage image;
	hr = LoadFromDDSMemory(buf, len, DDS_FLAGS_NONE, nullptr, image);
	if (FAILED(hr)) {
		printf("Error: cannot load dds file\n");
	}

	hr = CreateShaderResourceView(
		pRender->GetDevice(),
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		&m_pTexture
	);

	free(buf);

	if (FAILED(hr)) {
		printf("Error: cannot CreateShaderResourceView dds file\n");
	}

	return hr;
}

HRESULT Mesh::CreateDataBuffer(
	DXRender* pRender,
	float *pVertices,
	int verticesCount,
	unsigned int *pIndices,
	int indicesCount
) {
	HRESULT hr;

	D3D11_BUFFER_DESC bdv;
	ZeroMemory(&bdv, sizeof(bdv));
	bdv.Usage = D3D11_USAGE_DEFAULT;
	bdv.ByteWidth = sizeof(float) * verticesCount;
	bdv.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA datav;
	ZeroMemory(&datav, sizeof(datav));
	datav.pSysMem = pVertices;

	hr = pRender->GetDevice()->CreateBuffer(&bdv, &datav, &m_pVertexBuffer);
	if (FAILED(hr)) {
		printf("Error: cannot CreateBuffer vertex buffer\n");
		return hr;
	}

	D3D11_BUFFER_DESC bdi;
	ZeroMemory(&bdi, sizeof(bdi));
	bdi.Usage = D3D11_USAGE_DEFAULT;
	bdi.ByteWidth = sizeof(unsigned int) * indicesCount;
	bdi.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA datai;
	ZeroMemory(&datai, sizeof(datai));
	datai.pSysMem = pIndices;

	hr = pRender->GetDevice()->CreateBuffer(&bdi, &datai, &m_pIndexBuffer);
	if (FAILED(hr))
		printf("Error: cannot CreateBuffer index buffer\n");

	return hr;
}

HRESULT Mesh::Init(DXRender*pRender, float *pVertices, int verticesCount, unsigned int *pIndices, int indicesCount, D3D_PRIMITIVE_TOPOLOGY topology)
{
	HRESULT hr;

	m_hasAlpha = false;
	m_alphaCutout = false;
	m_pTexture = nullptr;
	m_pVertexBuffer = nullptr;
	m_pIndexBuffer = nullptr;
	m_pObjectBuffer = nullptr;
	m_countIndices = indicesCount;
	m_primitiveTopology = topology;
	m_World = XMMatrixIdentity();

	hr = EnsureSharedPipeline(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create shared pipeline\n");
		return hr;
	}
	s_sharedRefCount++;

	hr = CreateConstBuffer(pRender);
	if (FAILED(hr)) {
		printf("Error: cannot create const buffer\n");
		return hr;
	}

	hr = CreateDataBuffer(pRender, pVertices, verticesCount, pIndices, indicesCount);
	if (FAILED(hr)) {
		printf("Error: cannot create data buffer\n");
		return hr;
	}

	return hr;
}
