#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <DirectXMath.h>

#include "img_loader.hpp"
#include "renderware.h"

#include "GameModel.h"
#include "GameRender.h"
#include "GameCamera.h"
#include "GameInput.h"

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 1200
#define WINDOW_TITLE L"openvice"

using namespace DirectX; /* DirectXMath.h */

// utils
double countsPerSecond = 0.0;
__int64 CounterStart = 0;

int frameCount = 0;
int fps = 0;

__int64 frameTimeOld = 0;
double frameTime;

/*
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
}*/

/*
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

std::vector<Model*> gModels;

*/

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


void Render(GameRender *render, GameCamera *camera, GameModel *model, float time)
{
	render->RenderStart();

	model->Render(render, camera);

	/*for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->render();
	}*/

	render->RenderEnd();
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PWSTR pCmdLine, int nCmdShow)
{
	HWND hWnd = CreateWindowApp(hInstance, nCmdShow);
	ShowConsole();

	GameInput *gameInput = new GameInput();
	gameInput->Init(hInstance, hWnd);
	
	GameCamera *gameCamera = new GameCamera();
	gameCamera->Init(WINDOW_WIDTH, WINDOW_HEIGHT);
	
	GameRender *gameRender = new GameRender();
	gameRender->Init(hWnd);

	GameModel *gameModel = new GameModel();
	gameModel->Init(gameRender);

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


	//World = XMMatrixIdentity();
	//WVP = World * gameCamera->getView() * gameCamera->getProjection();

	float moveLeftRight = 0.0f;
	float moveBackForward = 0.0f;

	float camYaw = 0.0f;
	float camPitch = 0.0f;

	DIMOUSESTATE mouseLastState;
	DIMOUSESTATE mouseCurrState;

	mouseCurrState.lX = gameInput->GetMouseSpeedX();
	mouseCurrState.lY = gameInput->GetMouseSpeedY();

	mouseLastState.lX = gameInput->GetMouseSpeedX();
	mouseLastState.lY = gameInput->GetMouseSpeedY();

	// Главный цикл сообщений
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { // If have not messagess
			frameCount++;

			if (GetTime() > 1.0f) {
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

			mouseCurrState.lX = gameInput->GetMouseSpeedX();
			mouseCurrState.lY = gameInput->GetMouseSpeedY();

			if ((mouseCurrState.lX != mouseLastState.lX)
				|| (mouseCurrState.lY != mouseLastState.lY)) {

				camYaw += mouseLastState.lX * 0.001f;
				camPitch += mouseCurrState.lY * 0.001f;

				mouseLastState = mouseCurrState;
			}

			gameCamera->UpdateCamera(camPitch, camYaw, moveLeftRight, moveBackForward);

			Render(gameRender, gameCamera, gameModel, frameTime);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	gameRender->Cleanup();
	gameCamera->Cleanup();
	gameInput->Cleanup();

	delete gameRender;
	delete gameInput;
	delete gameCamera;

	return msg.wParam;
}