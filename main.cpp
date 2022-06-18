#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

#include "img_loader.hpp"
#include "renderware.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_TITLE L"openvice"

using namespace DirectX; // DirectXMath.h

ID3D11Device *g_pd3dDevice;
ID3D11DeviceContext *g_pImmediateContext;
IDXGISwapChain *g_pSwapChain;
ID3D11RenderTargetView *g_pRenderTargetView;

// ������

struct SimpleVertex
{
	float x, y, z;
};

ID3D11VertexShader *g_pVertexShader = NULL; // ��������� ������
ID3D11PixelShader *g_pPixelShader = NULL; // ���������� ������

ID3D11InputLayout *g_pVertexLayout = NULL; // �������� ������� ������

ID3D11Buffer *g_pVertexBuffer = NULL; // ����� ������
ID3D11Buffer *g_pIndexBuffer = NULL;

HRESULT InitGeometry(); // ������������� ������� ����� � ������ ������




ID3D11Buffer* cbPerObjectBuffer;

XMMATRIX WVP;
XMMATRIX World;
XMMATRIX camView;
XMMATRIX camProjection;

XMVECTOR camPosition;
XMVECTOR camTarget;
XMVECTOR camUp;

struct cbPerObject
{
	XMMATRIX  WVP;
};

cbPerObject cbPerObj;


void createConstBuffer()
{
	D3D11_BUFFER_DESC cbbd;
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerObject);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;


	g_pd3dDevice->CreateBuffer(&cbbd, NULL, &cbPerObjectBuffer);
}




// ��������������� ������� ��� ���������� �������� � D3DX11
HRESULT CompileShaderFromFile(LPCWSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

	hr = D3DCompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, NULL);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot compile shader", L"Error", MB_ICONERROR | MB_OK);
		return hr;
	}

	return S_OK;
}


class Model {

public:
	ID3D11Buffer *pVertexBuffer = NULL; // ����� ������
	ID3D11Buffer *pIndexBuffer = NULL;

	std::vector<rw::uint16> faces;
	std::vector<rw::float32> vertices;

	void render()
	{
		// Set the buffer.
		g_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// ��������� ������ ������
		UINT stride = sizeof(rw::float32) * 3;
		UINT offset = 0;
		g_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);

		// ��������� ������� ��������� ������ � ������
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// ���������� � ���������� ��������� �������
		g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
		g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

		// ���������� ��� �������
		g_pImmediateContext->DrawIndexed(faces.size(), 0, 0);
	};

	void cleanup()
	{

	};

	HRESULT createModel(std::vector<rw::uint16> faces, std::vector<rw::float32> vertices)
	{

		this->faces = faces;
		this->vertices = vertices;

		HRESULT hr = S_OK;

		D3D11_BUFFER_DESC bd;  // ���������, ����������� ����������� �����
		ZeroMemory(&bd, sizeof(bd));                    // ������� ��
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(rw::float32) * vertices.size(); // ������ ������ = ������ ����� ������� * 3
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;          // ��� ������ - ����� ������
		bd.CPUAccessFlags = 0;

		{
			D3D11_SUBRESOURCE_DATA InitData; // ���������, ���������� ������ ������
			ZeroMemory(&InitData, sizeof(InitData)); // ������� ��
			InitData.pSysMem = reinterpret_cast<char*>(vertices.data());               // ��������� �� ���� 3 �������

			// ����� ������ g_pd3dDevice ������� ������ ������ ������ ID3D11Buffer
			hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &pVertexBuffer);

			if (FAILED(hr)) {
				MessageBox(NULL, L"Cannot create buffer", L"Error", MB_OK);
				return hr;
			}
		}







		// Fill in a buffer description.
		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = sizeof(rw::uint16) * faces.size();
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;

		// Define the resource data.
		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = reinterpret_cast<char*>(faces.data());;
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		// Create the buffer with the device.
		hr = g_pd3dDevice->CreateBuffer(&bufferDesc, &InitData, &pIndexBuffer);
		if (FAILED(hr))
			return hr;

		// Set the buffer.
		g_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);







		return hr;
	}
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wp, lp);
	}

	return 0;
}

void InitViewport(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left; // �������� ������
	UINT height = rc.bottom - rc.top; // � ������ ����

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	// ���������� ������� � ��������� ����������
	UINT countViewports = 1;
	g_pImmediateContext->RSSetViewports(countViewports, &vp);
}

HRESULT CreateBackBuffer()
{
	// ������� ������ �����
	// RenderTargetOutput - ��� �������� �����, � RenderTargetView - ������
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create backbuffer", L"Error", MB_OK);
		return hr;
	}

	// ��������� g_pd3dDevice ������������ ��� �������� ���� ��������
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release(); // ������ ���� ������ �� �����������
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create render target view", L"Error", MB_OK);
		return hr;
	}

	// ���������� ������ ������� ������ � ��������� ����������
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	return hr;
}

HRESULT InitD3DX11(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left; // �������� ������
	UINT height = rc.bottom - rc.top; // � ������ ����

	/* �������� ��������� ������ � ����������� ��� � ������ ���� */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; // � ��� ���� ������ �����
	sd.BufferDesc.Width = width; // ������ ������
	sd.BufferDesc.Height = height; // ������ ������
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // ������ ������� � ������
	sd.BufferDesc.RefreshRate.Numerator = 60; // ������� ���������� ������
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // ���������� ������ - ������ �����
	sd.OutputWindow = hWnd; // ����������� � ������ ����
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // ������� �����

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};

	UINT arraySize = ARRAYSIZE(featureLevels);

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		featureLevels,
		arraySize,
		D3D11_SDK_VERSION,
		&sd,
		&g_pSwapChain,
		&g_pd3dDevice,
		NULL,
		&g_pImmediateContext
	);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create device", L"Error", MB_ICONERROR | MB_OK);
		return hr;
	}

	hr = CreateBackBuffer();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create back buffer", L"Error", MB_ICONERROR | MB_OK);
		return hr;
	}

	InitViewport(hWnd);

	return hr;
}


std::vector<Model*> gModels;

void Render()
{
	// ������� ������ �����
	float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; // �������, �������, �����, �����-�����
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);








	// ���������� � ���������� ��������� �������
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);




	//Set the World/View/Projection matrix, then send it to constant buffer in effect file
	World = XMMatrixIdentity();

	WVP = World * camView * camProjection;

	cbPerObj.WVP = XMMatrixTranspose(WVP);

	g_pImmediateContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);




	// ���������� ��� �������
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	/*for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->render();
	}*/

	// ��������� ������ ����� �� �����
	g_pSwapChain->Present(0, 0);
}

HWND CreateWindowApp(HINSTANCE hInstance, int nCmdShow)
{
	LPCWSTR CLASS_NAME = L"OpenViceWndClass";

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClass(&wc)) {
		MessageBox(NULL, L"Cannot register window", L"Error", MB_ICONERROR | MB_OK);
		return NULL;
	}

	RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// �������� ����
	HWND hWnd = CreateWindowEx(
		0, // ���. �����
		CLASS_NAME, // ����� ����
		WINDOW_TITLE, // �������� ����
		WS_OVERLAPPEDWINDOW, // �����
		CW_USEDEFAULT, CW_USEDEFAULT, // �������: x, y
		rc.right - rc.left, rc.bottom - rc.top, // ������: ������, ������
		NULL, // ������������ ����
		NULL, // ����
		hInstance, // Instance handle
		NULL // ���. ���������� ����������
	);

	if (hWnd == NULL) {
		MessageBox(NULL, L"Cannot create window", L"Error", MB_OK);
		return NULL;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

void ShowConsole()
{
	FILE* conin = stdin;
	FILE* conout = stdout;
	FILE* conerr = stderr;
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen_s(&conin, "CONIN$", "r", stdin);
	freopen_s(&conout, "CONOUT$", "w", stdout);
	freopen_s(&conerr, "CONOUT$", "w", stderr);
	SetConsoleTitle(L"appconsole");
}

void CleanupGeometry()
{
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();

	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
}

void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	CleanupGeometry();

	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

HRESULT CreatePixelShader()
{
	HRESULT hr = S_OK;

	// ���������� ����������� ������� �� �����
	// ID3DBlob* pPSBlob = NULL;
	// hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob);

	// ������������� ��� ����������������� PS �������
	ID3DBlob* pPSBlob = NULL;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot read compiled pixel shader", L"Error", MB_OK);
		return hr;
	}

	// �������� ����������� �������
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create pixel shader", L"Error", MB_OK);
		return hr;
	}

	return hr;
}

HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	// ���������� ���������� ������� �� �����
	// ID3DBlob* pVSBlob = NULL; // ��������������� ������
	// hr = CompileShaderFromFile(L"vertex_shader.hlsl", "VS", "vs_4_0", &pVSBlob);
	// if (FAILED(hr))	{
	// 	 MessageBox(NULL, L"Cannot compile vertex shader", L"Error", MB_OK);
	//	 return hr;
	// }

	// ������������� ����������������� VS �������
	ID3DBlob* pVSBlob = NULL;
	hr = D3DReadFileToBlob(L"vertex_shader.cso", &pVSBlob);
	if (FAILED(hr))	{
		MessageBox(NULL, L"Cannot read compiled vertex shader", L"Error", MB_OK);
		return hr;
	}

	// �������� ���������� �������
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr)) {
		pVSBlob->Release();
		MessageBox(NULL, L"Cannot create vertex shader", L"Error", MB_OK);
		return hr;
	}

	// ����������� ������� ������
	D3D11_INPUT_ELEMENT_DESC layout[] =	{
		/* ������������� ���, ������������� ������, ������, �������� ���� (0-15), ����� ������ ������ � ������ ������, ����� ��������� ����� (�� �����), InstanceDataStepRate (�� �����) */
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	// �������� ������� ������
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);

	pVSBlob->Release();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create input layout", L"Error", MB_OK);
		return hr;
	}

	// ����������� ������� ������
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// �������� ����������� �������
	CreatePixelShader();

	// �������� ������ ������ (��� ������� ������������)
	struct SimpleVertex vertices[4];

	vertices[0].x = -0.5f;  vertices[0].y = -0.5f;  vertices[0].z = 0.5f;
	vertices[1].x = -0.5f;  vertices[1].y = 0.5f;  vertices[1].z = 0.5f;
	vertices[2].x = 0.5f;  vertices[2].y = 0.5f;  vertices[2].z = 0.5f;
	vertices[3].x = 0.5f;  vertices[3].y = -0.5f;  vertices[3].z = 0.5f;

	D3D11_BUFFER_DESC bd;  // ���������, ����������� ����������� �����
	ZeroMemory(&bd, sizeof(bd));                    // ������� ��
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(struct SimpleVertex) * 4; // ������ ������ = ������ ����� ������� * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;          // ��� ������ - ����� ������
	bd.CPUAccessFlags = 0;

	{
		D3D11_SUBRESOURCE_DATA InitData; // ���������, ���������� ������ ������
		ZeroMemory(&InitData, sizeof(InitData)); // ������� ��
		InitData.pSysMem = vertices;               // ��������� �� ���� 3 �������

		// ����� ������ g_pd3dDevice ������� ������ ������ ������ ID3D11Buffer
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);

		if (FAILED(hr)) {
			MessageBox(NULL, L"Cannot create buffer", L"Error", MB_OK);
			return hr;
		}
	}






	

	// Create indices
	unsigned int indices[]  = {
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
	hr = g_pd3dDevice->CreateBuffer(&bufferDesc, &InitData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set the buffer.
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);





	// ��������� ������ ������
	UINT stride = sizeof(struct SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// ��������� ������� ��������� ������ � ������
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return hr;
}



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	HWND hWnd = CreateWindowApp(hInstance, nCmdShow);
	ShowConsole();

	HRESULT hr = InitD3DX11(hWnd);
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot init d3dx11", L"Error", MB_ICONERROR | MB_OK);
		return -1;
	}

	hr = InitGeometry();
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot init geometry", L"Error", MB_ICONERROR | MB_OK);
		return -1;
	}

	dir_file_open("E:/games/Grand Theft Auto Vice City/models/gta3.dir");
	img_file_open("E:/games/Grand Theft Auto Vice City/models/gta3.img");

	/* img_file_save_by_id(399); */

	dir_file_dump();
	// char *file = img_file_get(399);
	img_file_save_by_id(152);
	// dff_load(file);
	// free(file);

	std::ifstream in("C:/Files/projects/openvice/lawyer.dff", std::ios::binary);
	if (!in.is_open()) {
		MessageBox(NULL, L"Cannot open file", L"Error", MB_ICONERROR | MB_OK);
		return -1;
	}

	rw::Clump *clump = new rw::Clump();
	clump->read(in);

	clump->dump();

	

	/*for (uint32_t index = 0; index < clump->geometryList.size(); index++) {
		std::vector<rw::uint16> faces;

		

		for (uint32_t i = 0; i < clump->geometryList[index].faces.size() / 4; i++) {
			float f1 = clump->geometryList[index].faces[i * 4 + 0];
			faces.push_back(f1);

			float f2 = clump->geometryList[index].faces[i * 4 + 1];
			faces.push_back(f2);

			float f3 = clump->geometryList[index].faces[i * 4 + 2];
			faces.push_back(f3);

			float f4 = clump->geometryList[index].faces[i * 4 + 3];
			faces.push_back(f4);
		}

		std::vector <rw::float32> vertices;

		for (uint32_t i = 0; i < clump->geometryList[index].vertices.size() / 3; i++) {
			float x = clump->geometryList[index].vertices[i * 3 + 0];
			vertices.push_back(x);

			float y = clump->geometryList[index].vertices[i * 3 + 1];
			vertices.push_back(y);

			float z = clump->geometryList[index].vertices[i * 3 + 2];
			vertices.push_back(z);
		}

		
		//Model *model = new Model();
		//model->createModel(faces, vertices);

		//gModels.push_back(model);
		
	}*/

	clump->clear();
	delete clump;

	dir_file_close();
	img_file_close();

	createConstBuffer();

	camPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
	camTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	camUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	camView = XMMatrixLookAtLH(camPosition, camTarget, camUp);

	camProjection = XMMatrixPerspectiveFovLH(90.0f, (float)800 / 600, 1.0f, 1000.0f);

	World = XMMatrixIdentity();



	WVP = World * camView * camProjection;


	// ������� ���� ���������
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { // ���� ��������� ���
			Render(); // ������
		}
	}

	CleanupDevice();

	return msg.wParam;
}