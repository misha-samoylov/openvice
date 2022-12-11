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

ImgLoader *LoadImgFile()
{
	ImgLoader *imgLoader = new ImgLoader();
	imgLoader->Open(
		"C:/Games/Grand Theft Auto Vice City/models/gta3.img",
		"C:/Games/Grand Theft Auto Vice City/models/gta3.dir"
	);

	return imgLoader;
}

void FreeImgFile(ImgLoader *pImgLoader)
{
	pImgLoader->Cleanup();
	delete pImgLoader;
}

struct Mat {
	int index;
	std::string name;
	uint8_t *source;
	int size;
};
std::vector<Mat> g_materials;

void LoadTextureWithId(ImgLoader *pImgLoader, uint32_t fileId, int materialIndex)
{
	char *fileBuffer = pImgLoader->FileGetById(fileId);

	

	TextureDictionary txd;
	size_t offset = 0;
	txd.read(fileBuffer, &offset);

	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture &t = txd.texList[i];
		cout << i << " " << t.name << " " << t.maskName << " "
			<< " " << t.width[0] << " " << t.height[0] << " "
			<< " " << t.depth << " " << hex << t.rasterFormat << endl;

		if (txd.texList[i].dxtCompression)
			txd.texList[i].decompressDxt();

		txd.texList[i].convertTo32Bit();

		struct Mat m;
		m.index = materialIndex;
		m.source = *txd.texList[i].texels.data();
		m.size = txd.texList[i].dataSizes[0];
		g_materials.push_back(m);
	}

	// free(fileBuffer);
}

void LoadTextureWithName(ImgLoader *pImgLoader, const char *name, int materialIndex)
{
	std::string textureName = name;
	textureName += ".txd";

	int index = pImgLoader->FileGetIndexByName(textureName.c_str());
	if (index == -1)
		return;

	LoadTextureWithId(pImgLoader, index, materialIndex);
}

int LoadGameFileWithId(ImgLoader *pImgLoader, GameRender *render, uint32_t fileId)
{
	char *fileBuffer = pImgLoader->FileGetById(fileId);
	
	Clump *clump = new Clump();
	clump->Read(fileBuffer);
	clump->Dump();

	for (uint32_t index = 0; index < clump->GetGeometryList().size(); index++) {

		std::vector<float> gvertices;
		std::vector<float> gtexture;

		// Загружаем все материалы
		for (int i = 0; i < clump->GetGeometryList()[index].materialList.size(); i++) {
			Material material = clump->GetGeometryList()[index].materialList[i];

			std::cout << "Model material texture " << material.texture.name << std::endl;
			LoadTextureWithName(pImgLoader, material.texture.name.c_str(), i);
		}

		for (uint32_t i = 0; i < clump->GetGeometryList()[index].vertices.size() / 3; i++) {

			float x = clump->GetGeometryList()[index].vertices[i * 3 + 0];
			gvertices.push_back(x);

			float y = clump->GetGeometryList()[index].vertices[i * 3 + 1];
			gvertices.push_back(y);

			float z = clump->GetGeometryList()[index].vertices[i * 3 + 2];
			gvertices.push_back(z);

			// загружаем текстурные координаты
			if (clump->GetGeometryList()[index].flags & FLAGS_TEXTURED /*|| clump->GetGeometryList()[index].flags & FLAGS_TEXTURED2*/) {
				for (uint32_t j = 0; j < 1 /* clump->GetGeometryList()[index].numUVs */; j++) { // вставляем пока только  FLAGS_TEXTURED

					float tx = clump->GetGeometryList()[index].texCoords[j][i * 2 + 0]; /* index OR i ??? в последнем [] */
					float ty = clump->GetGeometryList()[index].texCoords[j][i * 2 + 1]; /* index OR i ??? в последнем [] */

					gtexture.push_back(tx);
					gtexture.push_back(ty);
				}
			}
			//else { // делаем загрушку на случай отсутвия текстуры
				//	gvertices.push_back(0.0);
				//	gvertices.push_back(0.0);
			//}
		}

		/* Пробегаемся по каждой модели	*/
		for (uint32_t i = 0; i < clump->GetGeometryList()[index].splits.size(); i++) {

			std::vector<uint32_t> gindices;

			/**
			 * Сохраняем индексы
			 */
			for (uint32_t j = 0; j < clump->GetGeometryList()[index].splits[i].indices.size(); j++) {
				uint32_t indices = clump->GetGeometryList()[index].splits[i].indices[j];
				gindices.push_back(indices);
			}

			/**
			 * Сохраняем в массив вершины и текстурные координаты,
			 * чтобы создать валидный вершинный буфер
			 */
			std::vector<float> vert;

			for (int v = 0; v < gvertices.size() / 3; v++) {
				float x = gvertices[v * 3 + 0];
				float y = gvertices[v * 3 + 1];
				float z = gvertices[v * 3 + 2];

				float tx = gtexture[v *2 + 0];
				float ty = gtexture[v * 2 + 1];

				vert.push_back(x);
				vert.push_back(y);
				vert.push_back(z);

				vert.push_back(tx);
				vert.push_back(ty);
			}

			//float *vertices = &gvertices[0]; /* convert to array float */
			//int countVertices = gvertices.size();
			float *vertices = &vert[0];
			int countVertices = vert.size();

			unsigned int *indices = &gindices[0];  /* convert to array unsigned int */
			int countIndices = clump->GetGeometryList()[index].splits[i].indices.size();

			
			
			D3D_PRIMITIVE_TOPOLOGY topology =
				clump->GetGeometryList()[index].faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			GameModel *gameModel = new GameModel();
			gameModel->Init(render, vertices, countVertices,
				indices, countIndices,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			uint32_t materialIndex = clump->GetGeometryList()[index].splits[i].matIndex;

			uint8_t *findedSrcTga;
			int siz = 0;
			for (int i = 0; i < g_materials.size(); i++) {
				if (g_materials[i].index == materialIndex) {
					findedSrcTga = g_materials[i].source;
					siz = g_materials[i].size;
				}
			}

			gameModel->setTgaFile(render, 
				&findedSrcTga, siz);

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

	ImgLoader *imgLoader = LoadImgFile();
	imgLoader->FileSaveById(152);

	//LoadTextureWithName(imgLoader, "radar01");
	//LoadTextureWithName(imgLoader, "radar02");
	//LoadTextureWithName(imgLoader, "radar03");
	//LoadTextureWithName(imgLoader, "radar04");
	LoadGameFileWithId(imgLoader, gameRender, 152);
	//LoadGameFileWithId(imgLoader, gameRender, 153);

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

	FreeImgFile(imgLoader);

	return msg.wParam;
}