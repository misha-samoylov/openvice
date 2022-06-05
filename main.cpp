#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <sstream>

#include <windows.h>
#include <d3d11.h>

#pragma comment (lib, "d3d11.lib")

#include "img_loader.hpp"
#include "renderware.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_TITLE L"openvice"

ID3D11Device *g_pd3dDevice;
ID3D11DeviceContext *g_pImmediateContext;
IDXGISwapChain *g_pSwapChain;
ID3D11RenderTargetView *g_pRenderTargetView;

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
	pBackBuffer->Release();
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

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		&featureLevel,
		1,
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

void CleanupDevice()
{
	// Сначала отключим контекст устройства, потом отпустим объекты.
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	// Порядок удаления имеет значение. Обратите внимание, мы удалеям
	// эти объекты порядке, обратном тому, в котором создавали.
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	HWND hWnd = CreateWindowApp(hInstance, nCmdShow);
	ShowConsole();

	HRESULT hr = InitD3DX11(hWnd);
	if (FAILED(hr)) {
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