#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <DirectXMath.h>

#include "renderware.h"

#include "ImgLoader.hpp"
#include "GameModel.hpp"
#include "GameRender.hpp"
#include "GameCamera.hpp"
#include "GameInput.hpp"
#include "GameWindow.hpp"
#include "GameUtils.hpp"

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 1200
#define WINDOW_TITLE L"openvice"

using namespace DirectX; /* DirectXMath.h */

std::vector<GameModel*> gModels;

int LoadGameFile(GameRender *render)
{
	/* ImgLoader *imgLoader = new ImgLoader();
	imgLoader->Open("D:/games/Grand Theft Auto Vice City/models/gta3.img",
		"D:/games/Grand Theft Auto Vice City/models/gta3.dir");
	imgLoader->FileSaveById(152);
	imgLoader->Cleanup();
	delete imgLoader; */

	std::ifstream in("C:/Users/john/Downloads/basketballcourt04.dff", std::ios::binary);
	if (!in.is_open()) {
		MessageBox(NULL, L"Cannot open file", L"Error", MB_ICONERROR | MB_OK);
		return -1;
	}

	rw::Clump *clump = new rw::Clump();
	clump->read(in);
	clump->dump();



	for (uint32_t index = 0; index < clump->geometryList.size(); index++) {

		std::vector<rw::uint16> gfaces;
		std::vector<float> gvertices;
		std::vector<unsigned int> gindices;
		
		for (uint32_t i = 0; i < clump->geometryList[index].faces.size() / 4; i++) {
			float f1 = clump->geometryList[index].faces[i * 4 + 0];
			gfaces.push_back(f1);

			float f2 = clump->geometryList[index].faces[i * 4 + 1];
			gfaces.push_back(f2);

			float f3 = clump->geometryList[index].faces[i * 4 + 2];
			gfaces.push_back(f3);

			float f4 = clump->geometryList[index].faces[i * 4 + 3];
			gfaces.push_back(f4);
		}

		for (uint32_t i = 0; i < clump->geometryList[index].splits.size(); i++) {
			//cout << endl << ind << "Split " << i << " {\n";
			//ind += "  ";
			//cout << ind << "matIndex: " << splits[i].matIndex << endl;
			//cout << ind << "numIndices: " << splits[i].indices.size() << endl;
			//cout << ind << "indices {\n";
			//if (!detailed)
			//	cout << ind + "  skipping\n";
			//else
			for (uint32_t j = 0; j < clump->geometryList[index].splits[i].indices.size(); j++)
				gindices.push_back(clump->geometryList[index].splits[i].indices[j]);
					//cout << ind + " " << splits[i].indices[j] << endl;
			//cout << ind << "}\n";
			//ind = ind.substr(0, ind.size() - 2);
			//cout << ind << "}\n";
		}

		for (uint32_t i = 0; i < clump->geometryList[index].vertices.size() / 3; i++) {
			float x = clump->geometryList[index].vertices[i * 3 + 0];
			gvertices.push_back(x);

			float y = clump->geometryList[index].vertices[i * 3 + 1];
			gvertices.push_back(y);

			float z = clump->geometryList[index].vertices[i * 3 + 2];
			gvertices.push_back(z);
		}

		float *vertices = &gvertices[0];
		int countVertices = gvertices.size();

		unsigned int *indices = &gindices[0];
		int countIndices = gindices.size();

		GameModel *gameModel = new GameModel();
		gameModel->Init(render, vertices, countVertices,
			indices, countIndices);

		gameModel->SetPrimitiveTopology(clump->geometryList[index].faceType == rw::FACETYPE_STRIP ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		gModels.push_back(gameModel);
	}

	clump->clear();
	delete clump;

	return 0;
}

void Render(GameRender *render, GameCamera *camera, float time)
{
	render->RenderStart();

	// model->Render(render, camera);

	for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->Render(render, camera);
	}

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

	/* create vertices */
	LoadGameFile(gameRender);

	/*float vertices[] = {
		-0.5f, -0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, -0.5f, 0.5f
	};
	int countVertices = sizeof(vertices) / sizeof(vertices[0]);*/

	/* create indices */
	/*unsigned int indices[] = {
		0, 1, 2,
		0, 2, 3,
	};
	int countIndices = sizeof(indices) / sizeof(indices[0]);*/

	/*float *vertices = &gvertices[0];
	int countVertices = gvertices.size();

	unsigned int *indices = &gindices[0];
	int countIndices = gindices.size();

	GameModel *gameModel = new GameModel();
	gameModel->Init(gameRender, vertices, countVertices, 
		indices, countIndices);*/

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

	/* main loop */
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT) {
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

			Render(gameRender, gameCamera, frameTime);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	gameRender->Cleanup();
	gameCamera->Cleanup();
	gameInput->Cleanup();
	//gameModel->Cleanup();

	delete gameCamera;
	delete gameInput;
//	delete gameModel;
	delete gameRender;

	return msg.wParam;
}