#include "GameModel.h"

HRESULT GameModel::CreateConstBuffer(GameRender *render)
{
	HRESULT hr;

	D3D11_BUFFER_DESC cbbd;
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerObject);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;

	hr = render->getDevice()->CreateBuffer(&cbbd, NULL, &cbPerObjectBuffer);
	return hr;
}

void GameModel::Cleanup()
{
	// clear buffers
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (cbPerObjectBuffer) cbPerObjectBuffer->Release();

	// clear layout
	if (g_pVertexLayout) g_pVertexLayout->Release();

	// clear shaders
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
}

void GameModel::Render(GameRender * render, GameCamera *camera)
{

	render->getDeviceContext()->IASetInputLayout(g_pVertexLayout);

	// Set the buffer
	render->getDeviceContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Установка буфера вершин
	UINT stride = sizeof(struct SimpleVertex);
	UINT offset = 0;
	render->getDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	render->getDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set shaders
	render->getDeviceContext()->VSSetShader(g_pVertexShader, NULL, 0);
	render->getDeviceContext()->PSSetShader(g_pPixelShader, NULL, 0);

	//XMMATRIX Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	//XMMATRIX Translation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	//World = Scale * Translation;

	WVP = World * camera->getView() * camera->getProjection();
	cbPerObj.WVP = XMMatrixTranspose(WVP);

	render->getDeviceContext()->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	render->getDeviceContext()->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

	// Render indexed vertices
	render->getDeviceContext()->DrawIndexed(6, 0, 0);
}


HRESULT GameModel::CreatePixelShader(GameRender *render)
{
	HRESULT hr = S_OK;

	// Compile shaders from file
	// ID3DBlob* pPSBlob = NULL;
	// hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob);

	ID3DBlob* pPSBlob = NULL;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot read compiled pixel shader", L"Error", MB_OK);
		return hr;
	}

	hr = render->getDevice()->CreatePixelShader(pPSBlob->GetBufferPointer(),
		pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);

	pPSBlob->Release();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create pixel shader", L"Error", MB_OK);
		return hr;
	}

	return hr;
}

HRESULT GameModel::CreateVertexShader(GameRender *render)
{
	HRESULT hr;

	hr = D3DReadFileToBlob(L"vertex_shader.cso", &pVSBlob);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot read compiled vertex shader", L"Error", MB_OK);
		return hr;
	}

	hr = render->getDevice()->CreateVertexShader(pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);

	if (FAILED(hr)) {
		pVSBlob->Release();
		MessageBox(NULL, L"Cannot create vertex shader", L"Error", MB_OK);
		return hr;
	}

	return hr;
}

HRESULT GameModel::CreateInputLayout(GameRender * render)
{
	HRESULT hr;

	// Определение шаблона вершин
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		/* семантическое имя, семантический индекс, размер, входящий слот (0-15), адрес начала данных в буфере вершин, класс входящего слота (не важно), InstanceDataStepRate (не важно) */
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	// Создание шаблона вершин
	hr = render->getDevice()->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create input layout", L"Error", MB_OK);
	}

	return hr;
}

HRESULT GameModel::CreateBufferModel(GameRender * render)
{
	HRESULT hr;

	struct SimpleVertex vertices[4];

	vertices[0].x = -0.5f;  vertices[0].y = -0.5f;  vertices[0].z = 0.5f;
	vertices[1].x = -0.5f;  vertices[1].y = 0.5f;  vertices[1].z = 0.5f;
	vertices[2].x = 0.5f;  vertices[2].y = 0.5f;  vertices[2].z = 0.5f;
	vertices[3].x = 0.5f;  vertices[3].y = -0.5f;  vertices[3].z = 0.5f;

	D3D11_BUFFER_DESC bd;  // Структура, описывающая создаваемый буфер
	ZeroMemory(&bd, sizeof(bd));                    // очищаем ее
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(struct SimpleVertex) * 4; // размер буфера = размер одной вершины * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;          // тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;

	{
		D3D11_SUBRESOURCE_DATA InitData; // Структура, содержащая данные буфера
		ZeroMemory(&InitData, sizeof(InitData)); // очищаем ее
		InitData.pSysMem = vertices;               // указатель на наши 3 вершины

		// Вызов метода g_pd3dDevice создаст объект буфера вершин ID3D11Buffer
		hr = render->getDevice()->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);

		if (FAILED(hr)) {
			MessageBox(NULL, L"Cannot create buffer", L"Error", MB_OK);
			return hr;
		}
	}

	// Create indices
	unsigned int indices[] = {
		0, 1, 2,
		0, 2, 3,
	};

	// Fill in a buffer description.
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(unsigned int) * 2 * 3;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	// Define the resource data.
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = indices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	// Create the buffer with the device.
	hr = render->getDevice()->CreateBuffer(&bufferDesc, &InitData, &g_pIndexBuffer);

	return hr;
}

HRESULT GameModel::Init(GameRender *render)
{
	HRESULT hr = S_OK;

	pVSBlob = NULL;

	WVP = XMMatrixIdentity();
	World = XMMatrixIdentity();

	CreateVertexShader(render);
	CreatePixelShader(render);
	CreateConstBuffer(render);
	CreateInputLayout(render);
	CreateBufferModel(render);

	pVSBlob->Release();

	return hr;
}