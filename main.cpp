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
#include "GameWindow.h"
#include "GameUtils.h"

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 1200
#define WINDOW_TITLE L"openvice"

using namespace DirectX; /* DirectXMath.h */

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

int temp()
{
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

	return 0;
}

void Render(GameRender *render, GameCamera *camera, GameModel *model, float time)
{
	render->RenderStart();
	model->Render(render, camera);

	/* for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->render();
	} */

	render->RenderEnd();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PWSTR pCmdLine, int nCmdShow)
{
	GameWindow *gameWindow = new GameWindow();
	gameWindow->Init(hInstance, nCmdShow, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

	GameInput *gameInput = new GameInput();
	gameInput->Init(hInstance, gameWindow->GetHandleWindow());
	
	GameCamera *gameCamera = new GameCamera();
	gameCamera->Init(WINDOW_WIDTH, WINDOW_HEIGHT);
	
	GameRender *gameRender = new GameRender();
	gameRender->Init(gameWindow->GetHandleWindow());

	GameModel *gameModel = new GameModel();
	gameModel->Init(gameRender);

	temp();

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

	int frameCount = 0;
	double frameTime;
	int fps = 0;

	// Главный цикл сообщений
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { // If have not messagess
			frameCount++;

			if (GameUtils::GetTime() > 1.0f) {
				fps = frameCount;
				frameCount = 0;
				GameUtils::StartTimer();
			}

			frameTime = GameUtils::GetFrameTime();

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

			gameCamera->Update(camPitch, camYaw, moveLeftRight, moveBackForward);

			Render(gameRender, gameCamera, gameModel, frameTime);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	gameRender->Cleanup();
	gameCamera->Cleanup();
	gameInput->Cleanup();
	gameModel->Cleanup();

	delete gameModel;
	delete gameCamera;
	delete gameInput;
	delete gameRender;

	return msg.wParam;
}