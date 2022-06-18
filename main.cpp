#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "img_loader.hpp"
#include "renderware.h"

#include "GameCamera.h"
#include "GameInput.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 1200
#define WINDOW_TITLE L"openvice"

using namespace DirectX; /* DirectXMath.h */

XMMATRIX WVP;
XMMATRIX World;


float moveLeftRight = 0.0f;
float moveBackForward = 0.0f;

float camYaw = 0.0f;
float camPitch = 0.0f;



ID3D11Device *g_pd3dDevice;
ID3D11DeviceContext *g_pImmediateContext;
IDXGISwapChain *g_pSwapChain;
ID3D11RenderTargetView *g_pRenderTargetView;

// Объект

struct SimpleVertex
{
	float x, y, z;
};

ID3D11VertexShader *g_pVertexShader = NULL; // Вершинный шейдер
ID3D11PixelShader *g_pPixelShader = NULL; // Пиксельный шейдер

ID3D11InputLayout *g_pVertexLayout = NULL; // Описание формата вершин

ID3D11Buffer *g_pVertexBuffer = NULL; // Буфер вершин
ID3D11Buffer *g_pIndexBuffer = NULL;

HRESULT InitGeometry(); // Инициализация шаблона ввода и буфера вершин


ID3D11Buffer* cbPerObjectBuffer;

struct cbPerObject
{
	XMMATRIX  WVP;
};

cbPerObject cbPerObj;

// utils
double countsPerSecond = 0.0;
__int64 CounterStart = 0;

int frameCount = 0;
int fps = 0;

__int64 frameTimeOld = 0;
double frameTime;

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

// Вспомогательная функция для компиляции шейдеров в D3DX11
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
	ID3D11Buffer *pVertexBuffer = NULL; // Буфер вершин
	ID3D11Buffer *pIndexBuffer = NULL;

	std::vector<rw::uint16> faces;
	std::vector<rw::float32> vertices;

	void render()
	{
		// Set the buffer.
		g_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// Установка буфера вершин
		UINT stride = sizeof(rw::float32) * 3;
		UINT offset = 0;
		g_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);

		// Установка способа отрисовки вершин в буфере
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Подключить к устройству рисования шейдеры
		g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
		g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

		// Нарисовать три вершины
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

		D3D11_BUFFER_DESC bd;  // Структура, описывающая создаваемый буфер
		ZeroMemory(&bd, sizeof(bd));                    // очищаем ее
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(rw::float32) * vertices.size(); // размер буфера = размер одной вершины * 3
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;          // тип буфера - буфер вершин
		bd.CPUAccessFlags = 0;

		{
			D3D11_SUBRESOURCE_DATA InitData; // Структура, содержащая данные буфера
			ZeroMemory(&InitData, sizeof(InitData)); // очищаем ее
			InitData.pSysMem = reinterpret_cast<char*>(vertices.data());               // указатель на наши 3 вершины

			// Вызов метода g_pd3dDevice создаст объект буфера вершин ID3D11Buffer
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
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	// Connect viewport to device context
	UINT countViewports = 1;
	g_pImmediateContext->RSSetViewports(countViewports, &vp);
}

HRESULT CreateBackBuffer()
{
	// RenderTargetOutput - front buffer
	// RenderTargetView - back buffer
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create backbuffer", L"Error", MB_OK);
		return hr;
	}

	// Device used for create all objects
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release(); // That's object does not needed
	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create render target view", L"Error", MB_OK);
		return hr;
	}

	// Connect back buffer to device context
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	return hr;
}

HRESULT InitD3DX11(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	/* Properties front buffer and attach it to window */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; // counts buffer = 1
	sd.BufferDesc.Width = width; // width buffer
	sd.BufferDesc.Height = height; // height buffer
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // pixel format in buffer
	sd.BufferDesc.RefreshRate.Numerator = 60; // частота обновления экрана
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // назначение буфера - задний буфер
	sd.OutputWindow = hWnd; // attach to window
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // windowed

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

void Render(GameCamera *gameCamera, float time)
{
	// Clear back buffer
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

	// Set shaders
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	XMMATRIX Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX Translation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	World = Scale * Translation;

	WVP = World * gameCamera->getView() * gameCamera->getProjection();

	cbPerObj.WVP = XMMatrixTranspose(WVP);

	g_pImmediateContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

	// Render indexed vertices
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	/*for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->render();
	}*/

	// Show back buffer to screen
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

	// Создание окна
	HWND hWnd = CreateWindowEx(
		0, // Доп. стили
		CLASS_NAME, // Класс окна
		WINDOW_TITLE, // Название окна
		WS_OVERLAPPEDWINDOW, // Стиль
		CW_USEDEFAULT, CW_USEDEFAULT, // Позиция: x, y
		rc.right - rc.left, rc.bottom - rc.top, // Размер: ширина, высота
		NULL, // Родительское окно
		NULL, // Меню
		hInstance, // Instance handle
		NULL // Доп. информация приложения
	);

	if (hWnd == NULL) {
		MessageBox(NULL, L"Cannot create window", L"Error", MB_OK);
		return NULL;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	ShowCursor(FALSE);

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
	// clear buffers
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	
	// clear layout
	if (g_pVertexLayout) g_pVertexLayout->Release();

	// clear shaders
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

	// Компиляция пиксельного шейдера из файла
	// ID3DBlob* pPSBlob = NULL;
	// hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob);

	// Использование уже скомпилированного PS шейдера
	ID3DBlob* pPSBlob = NULL;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &pPSBlob);

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot read compiled pixel shader", L"Error", MB_OK);
		return hr;
	}

	// Создание пиксельного шейдера
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

	// Компиляция вершинного шейдера из файла
	// ID3DBlob* pVSBlob = NULL; // Вспомогательный объект
	// hr = CompileShaderFromFile(L"vertex_shader.hlsl", "VS", "vs_4_0", &pVSBlob);
	// if (FAILED(hr))	{
	// 	 MessageBox(NULL, L"Cannot compile vertex shader", L"Error", MB_OK);
	//	 return hr;
	// }

	// Использование скомпилированного VS шейдера
	ID3DBlob* pVSBlob = NULL;
	hr = D3DReadFileToBlob(L"vertex_shader.cso", &pVSBlob);
	if (FAILED(hr))	{
		MessageBox(NULL, L"Cannot read compiled vertex shader", L"Error", MB_OK);
		return hr;
	}

	// Создание вершинного шейдера
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr)) {
		pVSBlob->Release();
		MessageBox(NULL, L"Cannot create vertex shader", L"Error", MB_OK);
		return hr;
	}

	// Определение шаблона вершин
	D3D11_INPUT_ELEMENT_DESC layout[] =	{
		/* семантическое имя, семантический индекс, размер, входящий слот (0-15), адрес начала данных в буфере вершин, класс входящего слота (не важно), InstanceDataStepRate (не важно) */
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	// Создание шаблона вершин
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);

	pVSBlob->Release();

	if (FAILED(hr)) {
		MessageBox(NULL, L"Cannot create input layout", L"Error", MB_OK);
		return hr;
	}

	// Подключение шаблона вершин
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Создание пиксельного шейдера
	CreatePixelShader();

	// Создание буфера вершин (три вершины треугольника)
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

	// Set the buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);



	// Установка буфера вершин
	UINT stride = sizeof(struct SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Установка способа отрисовки вершин в буфере
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return hr;
}



void StartTimer()
{
	LARGE_INTEGER frequencyCount;
	QueryPerformanceFrequency(&frequencyCount);

	countsPerSecond = double(frequencyCount.QuadPart);

	QueryPerformanceCounter(&frequencyCount);
	CounterStart = frequencyCount.QuadPart;
}

double GetTime()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	return double(currentTime.QuadPart - CounterStart) / countsPerSecond;
}

double GetFrameTime()
{
	LARGE_INTEGER currentTime;
	__int64 tickCount;
	QueryPerformanceCounter(&currentTime);

	tickCount = currentTime.QuadPart - frameTimeOld;
	frameTimeOld = currentTime.QuadPart;

	if (tickCount < 0.0f)
		tickCount = 0.0f;

	return float(tickCount) / countsPerSecond;
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

	GameInput *gameInput = new GameInput();
	GameCamera *gameCamera = new GameCamera();

	gameCamera->Init(WINDOW_WIDTH, WINDOW_HEIGHT);
	

	if (gameInput->Init(hInstance, hWnd) == S_FALSE)
	{
		MessageBox(0, L"Direct Input Initialization - Failed",
			L"Error", MB_OK);
		return 1;
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

	
	World = XMMatrixIdentity();
	WVP = World * gameCamera->getView() * gameCamera->getProjection();

	// Главный цикл сообщений
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { // If have not messagess
			frameCount++;
			if (GetTime() > 1.0f)
			{
				fps = frameCount;
				frameCount = 0;
				StartTimer();
			}

			frameTime = GetFrameTime();

			gameInput->Detect();

			float speed = 10.0f * frameTime;

			if (gameInput->IsKey(DIK_ESCAPE)) {
				PostQuitMessage(-1);
			}

			if (gameInput->IsKey(DIK_W)) {
				moveBackForward += speed;
			}

			if (gameInput->IsKey(DIK_A)) {
				moveLeftRight -= speed;
			}

			if (gameInput->IsKey(DIK_S)) {
				moveBackForward -= speed;
			}

			if (gameInput->IsKey(DIK_D)) {
				moveLeftRight += speed;
			}

			gameCamera->UpdateCamera(camPitch, camYaw, moveLeftRight, moveBackForward);

			Render(gameCamera, frameTime);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	CleanupDevice();

	gameCamera->Cleanup();
	gameInput->Cleanup();

	delete gameInput;
	delete gameCamera;

	return msg.wParam;
}