#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <sstream>

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

#include "img_loader.hpp"
#include "renderware.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_TITLE L"openvice"

ID3D11Device *g_pd3dDevice;
ID3D11DeviceContext *g_pImmediateContext;
IDXGISwapChain *g_pSwapChain;
ID3D11RenderTargetView *g_pRenderTargetView;

// object

struct SimpleVertex
{
	float x, y, z;
};
ID3D11VertexShader*     g_pVertexShader = NULL;             // Вершинный шейдер
ID3D11PixelShader*      g_pPixelShader = NULL;        // Пиксельный шейдер
ID3D11InputLayout*      g_pVertexLayout = NULL;             // Описание формата вершин
ID3D11Buffer*         g_pVertexBuffer = NULL;         // Буфер вершин

HRESULT InitGeometry();    // Инициализация шаблона ввода и буфера вершин

//--------------------------------------------------------------------------------------

// Вспомогательная функция для компиляции шейдеров в D3DX11

//--------------------------------------------------------------------------------------

HRESULT CompileShaderFromFile(LPCWSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	//ID3DBlob* pErrorBlob;

	hr = D3DCompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, NULL);

	if (FAILED(hr)) {
		//if (pErrorBlob != NULL)
		//	OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

		//if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}

	//if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

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
	UINT width = rc.right - rc.left; // получаем ширину
	UINT height = rc.bottom - rc.top; // и высоту окна

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	// Подключаем вьюпорт к контексту устройства
	g_pImmediateContext->RSSetViewports(1, &vp);
}

HRESULT CreateBackBuffer()
{
	// Создаем задний буфер. Обратите внимание, в SDK
	// RenderTargetOutput - это передний буфер, а RenderTargetView - задний.
	ID3D11Texture2D* pBackBuffer = NULL;
	HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	if (FAILED(hr)) {
		return hr;
	}

	// Интерфейс g_pd3dDevice используется для создания всех объектов
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release(); // Больше этот объект не потребуется
	if (FAILED(hr)) {
		return hr;
	}

	// Подключаем объект заднего буфера к контексту устройства
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	return hr;
}

HRESULT InitD3DX11(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left; // получаем ширину
	UINT height = rc.bottom - rc.top; // и высоту окна

	/* свойства переднего буфера и привязывает его к нашему окну */
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; // у нас один задний буфер
	sd.BufferDesc.Width = width; // ширина буфера
	sd.BufferDesc.Height = height; // высота буфера
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // формат пикселя в буфере
	sd.BufferDesc.RefreshRate.Numerator = 60; // частота обновления экрана
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // назначение буфера - задний буфер
	sd.OutputWindow = hWnd; // привязываем к нашему окну
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE; // не полноэкранный режим (оконный)

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

void Render()
{
	// Очищаем задний буфер
	float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; // красный, зеленый, синий, альфа-канал
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);


	// Подключить к устройству рисования шейдеры

	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);

	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	// Нарисовать три вершины

	g_pImmediateContext->Draw(3, 0);


	// Выбросить задний буфер на экран
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
	if( g_pVertexBuffer ) g_pVertexBuffer->Release();
	if( g_pVertexLayout ) g_pVertexLayout->Release();

	if( g_pVertexShader ) g_pVertexShader->Release();
	if( g_pPixelShader ) g_pPixelShader->Release();
}

void CleanupDevice()
{
	// Сначала отключим контекст устройства, потом отпустим объекты
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	CleanupGeometry();

	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}



HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	// Компиляция вершинного шейдера из файла
	ID3DBlob* pVSBlob = NULL; // Вспомогательный объект - просто место в оперативной памяти
	hr = CompileShaderFromFile(L"vertex_shader.hlsl", "VS", "vs_4_0", &pVSBlob);

	if (FAILED(hr))	{
		MessageBox(NULL, L"Cannot compile vertex shader", L"Error", MB_OK);
		return hr;
	}

	// Создание вершинного шейдера
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr)) {
		pVSBlob->Release();
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

	if (FAILED(hr)) 
		return hr;

	// Подключение шаблона вершин
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Компиляция пиксельного шейдера из файла
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"pixel_shader.hlsl", "PS", "ps_4_0", &pPSBlob);

	if (FAILED(hr))	{
		MessageBox(NULL, L"Cannot compile pixel shader", L"Error", MB_OK);
		return hr;
	}

	// Создание пиксельного шейдера
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();

	if (FAILED(hr))
		return hr;

	// Создание буфера вершин (три вершины треугольника)
	SimpleVertex vertices[3];

	vertices[0].x = 0.0f;  vertices[0].y = 0.5f;  vertices[0].z = 0.5f;
	vertices[1].x = 0.5f;  vertices[1].y = -0.5f;  vertices[1].z = 0.5f;
	vertices[2].x = -0.5f;  vertices[2].y = -0.5f;  vertices[2].z = 0.5f;

	D3D11_BUFFER_DESC bd;  // Структура, описывающая создаваемый буфер
	ZeroMemory(&bd, sizeof(bd));                    // очищаем ее
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 3; // размер буфера = размер одной вершины * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;          // тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData; // Структура, содержащая данные буфера
	ZeroMemory(&InitData, sizeof(InitData)); // очищаем ее
	InitData.pSysMem = vertices;               // указатель на наши 3 вершины

	// Вызов метода g_pd3dDevice создаст объект буфера вершин ID3D11Buffer
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);

	if (FAILED(hr)) 
		return hr;

	// Установка буфера вершин:
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;

	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Установка способа отрисовки вершин в буфере
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	HWND hWnd = CreateWindowApp(hInstance, nCmdShow);
	ShowConsole();

	HRESULT hr = InitD3DX11(hWnd);
	if (FAILED(hr)) {
		return -1;
	}

	InitGeometry();

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
		printf("cannot open file\n");
		return -1;
	}

	rw::Clump *clump = new rw::Clump();
	clump->read(in);

	clump->dump();
	clump->clear();
	delete clump;

	dir_file_close();
	img_file_close();

	// Главный цикл сообщений
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { // Если сообщений нет
			Render(); // Рисуем
		}
	}

	CleanupDevice();

	return msg.wParam;
}