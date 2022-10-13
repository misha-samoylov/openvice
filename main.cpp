#include <iostream>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <DirectXMath.h>

#include "renderware.h"

#include "loaders/ImgLoader.hpp"
#include "loaders/Clump.h"
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

void LoadTxdFile()
{
	/*ifstream rw(argv[1], ios::binary);
	TextureDictionary txd;
	txd.read(rw);
	rw.close();
	for (uint32 i = 0; i < txd.texList.size(); i++) {
		NativeTexture &t = txd.texList[i];
		cout << i << " " << t.name << " " << t.maskName << " "
			<< " " << t.width[0] << " " << t.height[0] << " "
			<< " " << t.depth << " " << hex << t.rasterFormat << endl;
		if (txd.texList[i].platform == PLATFORM_PS2)
			txd.texList[i].convertFromPS2(0x40);
		if (txd.texList[i].platform == PLATFORM_XBOX)
			txd.texList[i].convertFromXbox();
		if (txd.texList[i].dxtCompression)
			txd.texList[i].decompressDxt();
		txd.texList[i].convertTo32Bit();
		txd.texList[i].writeTGA();
	}*/
}

int LoadGameFile(GameRender *render)
{
	ImgLoader *imgLoader = new ImgLoader();
	imgLoader->Open("C:/Games/Grand Theft Auto Vice City/models/gta3.img",
		"C:/Games/Grand Theft Auto Vice City/models/gta3.dir");
	//imgLoader->FileSaveById(152);
	char *fileBuffer = imgLoader->FileGetById(152);
	imgLoader->Cleanup();
	delete imgLoader;

	//std::ifstream in("C:/Users/john/Downloads/basketballcourt04.dff", std::ios::binary);
	//if (!in.is_open()) {
	//	MessageBox(NULL, L"Cannot open file", L"Error", MB_ICONERROR | MB_OK);
	//	return -1;
	//}

	Clump *clump = new Clump();
	//clump->Read(in);
	clump->Read(fileBuffer);
	clump->Dump();
	
	for (uint32_t index = 0; index < clump->GetGeometryList().size(); index++) {

		std::vector<float> gvertices;

		for (uint32_t i = 0; i < clump->GetGeometryList()[index].vertices.size() / 3; i++) {

			float x = clump->GetGeometryList()[index].vertices[i * 3 + 0];
			gvertices.push_back(x);

			float y = clump->GetGeometryList()[index].vertices[i * 3 + 1];
			gvertices.push_back(y);

			float z = clump->GetGeometryList()[index].vertices[i * 3 + 2];
			gvertices.push_back(z);
		}

		for (uint32_t i = 0; i < clump->GetGeometryList()[index].splits.size(); i++) {

			std::vector<uint32_t> gindices;

			for (uint32_t j = 0; j < clump->GetGeometryList()[index].splits[i].indices.size(); j++) {
				gindices.push_back(clump->GetGeometryList()[index].splits[i].indices[j]);
			}

			float *vertices = &gvertices[0]; /* convert to array float */
			int countVertices = gvertices.size();

			unsigned int *indices = &gindices[0];  /* convert to array unsigned int */
			int countIndices = clump->GetGeometryList()[index].splits[i].indices.size();

			D3D_PRIMITIVE_TOPOLOGY topology =
				clump->GetGeometryList()[index].faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			GameModel *gameModel = new GameModel();
			gameModel->Init(render, vertices, countVertices,
				indices, countIndices,
				topology);

			gModels.push_back(gameModel);
		}
	}

	clump->Clear();
	delete clump;

	free(fileBuffer);

	return 0;
}

void Render(GameRender *render, GameCamera *camera)
{
	render->RenderStart();

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

	LoadGameFile(gameRender);

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


	ImgLoader *imgLoader = new ImgLoader();
	imgLoader->Open("C:/Games/Grand Theft Auto Vice City/models/gta3.img",
		"C:/Games/Grand Theft Auto Vice City/models/gta3.dir");
	//imgLoader->FileSaveById(152);
	char *fileBuffer = imgLoader->FileGetById(1);
	imgLoader->Cleanup();
	delete imgLoader;


	TextureDictionary txd;
	size_t offset = 0;
	txd.read(fileBuffer, &offset);
	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture &t = txd.texList[i];
		cout << i << " " << t.name << " " << t.maskName << " "
			<< " " << t.width[0] << " " << t.height[0] << " "
			<< " " << t.depth << " " << hex << t.rasterFormat << endl;
		//if (txd.texList[i].platform == PLATFORM_PS2)
		//	txd.texList[i].convertFromPS2(0x40);
		//if (txd.texList[i].platform == PLATFORM_XBOX)
		//	txd.texList[i].convertFromXbox();
		if (txd.texList[i].dxtCompression)
			txd.texList[i].decompressDxt();
		txd.texList[i].convertTo32Bit();
		//txd.texList[i].writeTGA();
	}



	/* main loop */
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else { /* if have not messages */
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

			Render(gameRender, gameCamera);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	gameRender->Cleanup();
	gameCamera->Cleanup();
	gameInput->Cleanup();
	for (int i = 0; i < gModels.size(); i++) {
		gModels[i]->Cleanup();
		delete gModels[i];
	}

	delete gameCamera;
	delete gameInput;
	delete gameRender;

	return msg.wParam;
}