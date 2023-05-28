#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <windows.h>
#include <DirectXMath.h>

#include "renderware.h"

#include "loaders/ImgLoader.hpp"
#include "loaders/Clump.h"
#include "Mesh.hpp"
#include "GameRender.hpp"
#include "GameCamera.hpp"
#include "GameInput.hpp"
#include "GameWindow.hpp"
#include "GameUtils.hpp"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE L"openvice"

using namespace DirectX;

/* IPL file contains model position */
struct IPLFile {
	int id;
	std::string modelName;
	int interior;
	float posX, posY, posZ;
	float scale[3];
	float rot[4];
};

/* IDE file contains model name and their texture name */
struct IDEFile {
	int objectId;
	std::string modelName;
	std::string textureArchiveName;
};

struct GameMaterial {
	std::string name; /* without extension ".TXD" */
	uint8_t* source;
	int size;
	uint32_t width;
	uint32_t height;
	uint32_t dxtCompression;
	uint32_t depth;
};

struct ModelMaterial {
	std::string materialName;
	int index;
};

std::vector<IDEFile> g_ideFile;
std::vector<IPLFile> g_MapObjects;
std::vector<Mesh*> g_LoadedModels;
std::vector<GameMaterial> g_Textures;


template <typename T>
void remove_duplicates(std::vector<T>& vec)
{
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

void LoadAllTexturesFromTXDFile(ImgLoader *pImgLoader, const char *filename)
{
	std::string textureName = filename;
	textureName += ".txd";

	int fileId = pImgLoader->GetFileIndexByName(textureName.c_str());
	if (fileId == -1) {
		printf("[ERROR] Cannot find file %s in IMG archive\n", textureName.c_str());
		return;
	}

	printf("[OK] Finded file %s in IMG archive\n", textureName.c_str());

	char *fileBuffer = pImgLoader->GetFileById(fileId);

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);

	/* Loop for every texture in TXD file */
	for (uint32_t i = 0; i < txd.texList.size(); i++) {

		/* TODO: Check for already loaded texture */

		NativeTexture &t = txd.texList[i];
		cout << i << " " << t.name << " " << t.maskName << " "
			<< " " << t.width[0] << " " << t.height[0] << " "
			<< " " << t.depth << " " << hex << t.rasterFormat << endl;

		uint8_t* texelsToArray = txd.texList[i].texels[0];
		size_t len = txd.texList[i].dataSizes[0];

		struct GameMaterial m;
		m.name = t.name; /* without extension ".TXD" */

		/* TODO: Replace copy to buffer to best solution */
		m.source = (uint8_t *)malloc(len);
		memcpy(m.source, texelsToArray, len);

		m.size = txd.texList[i].dataSizes[0];
		m.width = txd.texList[i].width[0];
		m.height = txd.texList[i].height[0];
		m.dxtCompression = txd.texList[i].dxtCompression; /* DXT1, DXT3, DXT4 */
		m.depth = txd.texList[i].depth;

		printf("[OK] Loaded texture name %s from TXD file %s\n", t.name.c_str(), textureName.c_str());

		g_Textures.push_back(m);
	}

	free(fileBuffer);
}

int LoadFileDFFWithName(ImgLoader* pImgLoader, GameRender* render, std::string name, int modelId)
{
	int fileId = pImgLoader->GetFileIndexByName(name.c_str());
	if (fileId == -1) {
		printf("[ERROR] Cannot find %s.dff in IMG archive\n", name.c_str());
		return 1;
	}

	if (strstr(name.c_str(), "LOD")) {
		printf("[NOTICE] Skip loading LOD file: %s\n", name.c_str());
		return 1;
	}

	char* fileBuffer = pImgLoader->GetFileById(fileId);

	Clump* clump = new Clump();
	clump->Read(fileBuffer);

	for (uint32_t index = 0; index < clump->GetGeometryList().size(); index++) {

		std::vector<ModelMaterial> materIndex;

		std::vector<float> modelVertices;
		std::vector<float> modelTextureCoord;

		/* Load all materials */
		for (int i = 0; i < clump->GetGeometryList()[index].materialList.size(); i++) {
			Material material = clump->GetGeometryList()[index].materialList[i];

			struct ModelMaterial matInd;
			matInd.materialName = material.texture.name;
			matInd.index = i;
			materIndex.push_back(matInd);
		}

		for (uint32_t i = 0; i < clump->GetGeometryList()[index].vertices.size() / 3; i++) {

			float x = clump->GetGeometryList()[index].vertices[i * 3 + 0];
			modelVertices.push_back(x);

			float y = clump->GetGeometryList()[index].vertices[i * 3 + 1];
			modelVertices.push_back(y);

			float z = clump->GetGeometryList()[index].vertices[i * 3 + 2];
			modelVertices.push_back(z);

			/* Load texture coordinates model */
			if (clump->GetGeometryList()[index].flags & FLAGS_TEXTURED /*|| clump->GetGeometryList()[index].flags & FLAGS_TEXTURED2*/) {
				for (uint32_t j = 0; j < 1 /* clump->GetGeometryList()[index].numUVs */; j++) { /* insert now FLAGS_TEXTURED */

					float tx = clump->GetGeometryList()[index].texCoords[j][i * 2 + 0];
					float ty = clump->GetGeometryList()[index].texCoords[j][i * 2 + 1];

					modelTextureCoord.push_back(tx);
					modelTextureCoord.push_back(ty);
				}
			}
		}

		/* Loop for every mesh */
		for (uint32_t i = 0; i < clump->GetGeometryList()[index].splits.size(); i++) {

			std::vector<uint32_t> meshIndices;

			/* Save indices */
			for (uint32_t j = 0; j < clump->GetGeometryList()[index].splits[i].indices.size(); j++) {
				uint32_t indices = clump->GetGeometryList()[index].splits[i].indices[j];
				meshIndices.push_back(indices);
			}

			/* Save to data for create vertex buffer (x,y,z tx,ty) */
			std::vector<float> meshVertexData;

			for (int v = 0; v < modelVertices.size() / 3; v++) {
				float x = modelVertices[v * 3 + 0];
				float y = modelVertices[v * 3 + 1];
				float z = modelVertices[v * 3 + 2];

				float tx = modelTextureCoord[v * 2 + 0];
				float ty = modelTextureCoord[v * 2 + 1];

				/*
				 * Flip coordinates. We use Left Handed Coordinates,
				 * but GTA engine use own coordinate system:
				 * X Ц east/west direction
				 * Y Ц north/south direction
				 * Z Ц up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				meshVertexData.push_back(y);
				meshVertexData.push_back(z);
				meshVertexData.push_back(x);

				meshVertexData.push_back(tx);
				meshVertexData.push_back(ty);
			}

			float* vertices = &meshVertexData[0];
			int countVertices = meshVertexData.size();

			unsigned int* indices = &meshIndices[0]; /* Convert to array unsigned int */
			int countIndices = clump->GetGeometryList()[index].splits[i].indices.size();

			D3D_PRIMITIVE_TOPOLOGY topology =
				clump->GetGeometryList()[index].faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			Mesh* gameModel = new Mesh();

			gameModel->Init(render, vertices, countVertices,
				indices, countIndices,
				topology
			);

			uint32_t materialIndex = clump->GetGeometryList()[index].splits[i].matIndex;

			int index = -1;

			// поиск текстуры по индексу
			for (int ib = 0; ib < materIndex.size(); ib++) {

				if (materialIndex == materIndex[ib].index) {

					for (int im = 0; im < g_Textures.size(); im++) {
						if (g_Textures[im].name == materIndex[ib].materialName) {
							index = im;
							break;
						}
					}

				}
			}

			if (index != -1) {
				gameModel->SetDataDDS(
					render,
					g_Textures[index].source,
					g_Textures[index].size,
					g_Textures[index].width,
					g_Textures[index].height,
					g_Textures[index].dxtCompression,
					g_Textures[index].depth /* TODO: depth is not working */
				);
			}
			gameModel->SetModelName(name);
			gameModel->SetModelId(modelId);

			g_LoadedModels.push_back(gameModel);
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

	// ѕолучаем местоположени€ объектов на карте
	for (int i = 0; i < g_MapObjects.size(); i++) {
		// ѕроходимс€ по загруженным модел€м
		for (int m = 0; m < g_LoadedModels.size(); m++) {
			// ≈сли нашли модель, то ставим ей координаты и рисуем
			if (g_MapObjects[i].id == g_LoadedModels[m]->GetModelId()) {
				g_LoadedModels[m]->SetPosition(
					g_MapObjects[i].posX, g_MapObjects[i].posY, g_MapObjects[i].posZ,
					g_MapObjects[i].scale[0], g_MapObjects[i].scale[1], g_MapObjects[i].scale[2],
					g_MapObjects[i].rot[0], g_MapObjects[i].rot[1], g_MapObjects[i].rot[2], g_MapObjects[i].rot[3]
				);
				g_LoadedModels[m]->Render(render, camera);
			}
		}
	}

	render->RenderEnd();
}

void LoadIDEFile(const char* filepath)
{
	FILE* fp;
	char str[512];
	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("Cannot open file.\n");
		return;
	}

	bool isObjs = false;

	while (!feof(fp)) {
		if (fgets(str, 512, fp)) {

			if (strcmp(str, "objs\n") == 0) {
				isObjs = true;
			}

			if (strcmp(str, "end") == 0) {
				if (isObjs) {
					isObjs = false;
				}
			}

			int id = 0;
			char modelName[64];
			char textureArchiveName[64];
			//std::string modelNameq;
			int interior = 0;
			float posX = 0, posY = 0, posZ = 0;
			float scale[3];
			float rot[4];

			int values = sscanf(str, "%d, %64[^,], %64[^,]", &id, modelName, textureArchiveName);

			printf("%s", str);
			if (values == 3 && isObjs) {

				// ƒобавл€ем .dff окончание, так как там указано без DFF формата
				//modelNameq = modelNameq + ".dff";

				std::string mName = modelName;
				mName = mName + ".dff";

				std::string taName = textureArchiveName;
				//taName = taName + ".txd";

				struct IDEFile idf;
				idf.objectId = id;
				idf.modelName = mName;
				idf.textureArchiveName = taName;
				
				g_ideFile.push_back(idf);
			}
		}

	}

	fclose(fp);
}

void LoadIPLFile(const char *filepath)
{
	FILE* fp;
	char str[512];
	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("Cannot open file.\n");
		return;
	}

	bool isObjs = false;

	while (!feof(fp)) {
		if (fgets(str, 512, fp)) {

			if (strcmp(str, "inst\n") == 0) {
				isObjs = true;
			}

			if (strcmp(str, "end") == 0) {
				if (isObjs) {
					isObjs = false;
				}
			}


			int id = 0;
			char modelName[64];
			//std::string modelNameq;
			int interior = 0;
			float posX = 0, posY = 0, posZ = 0;
			float scale[3];
			float rot[4];

			int values = sscanf(str, "%d, %64[^,], %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", &id, modelName, &interior, &posX, &posY, &posZ,
				&scale[0], &scale[1], &scale[2],
				&rot[0], &rot[1], &rot[2], &rot[3]
			);

			printf("%s", str);
			if (values == 13 && isObjs) {

				// ƒобавл€ем .dff окончание, так как там указано без DFF формата
				//modelNameq = modelNameq + ".dff";

				std::string mName = modelName;
				mName = mName + ".dff";
								
				struct IPLFile iplfile;
				iplfile.id = id;
				iplfile.modelName = mName;
				iplfile.interior = interior;

				/*
				 * ћен€ем положение модели в пространстве так как наша камера
				 * в Left Handed Coordinates, а движок GTA в своей координатной системе:
				 * X Ц east/west direction
				 * Y Ц north/south direction
				 * Z Ц up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				iplfile.posX = posY;
				iplfile.posY = posZ;
				iplfile.posZ = posX;

				iplfile.scale[0] = scale[1]; // y 
				iplfile.scale[1] = scale[2]; // z
				iplfile.scale[2] = scale[0]; // x

				iplfile.rot[0] = rot[0];
				iplfile.rot[1] = rot[1];
				iplfile.rot[2] = rot[2];
				iplfile.rot[3] = rot[3];

				g_MapObjects.push_back(iplfile);
			}
		}
			
	}

	fclose(fp);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, INT nCmdShow)
{
	GameWindow *gameWindow = new GameWindow();
	gameWindow->Init(hInstance, nCmdShow, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

	GameInput *gameInput = new GameInput();
	gameInput->Init(hInstance, gameWindow->GetHandleWindow());
	
	GameCamera *gameCamera = new GameCamera();
	gameCamera->Init(WINDOW_WIDTH, WINDOW_HEIGHT);
	
	GameRender *gameRender = new GameRender();
	gameRender->Init(gameWindow->GetHandleWindow());

	ImgLoader* imgLoader = new ImgLoader();
	imgLoader->Open(
		"C:/Games/Grand Theft Auto Vice City/models/gta3.img",
		"C:/Games/Grand Theft Auto Vice City/models/gta3.dir"
	);

	/* Load map models and their textures */
	LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ide");
	LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/bank/bank.ide");


	/* Load from IDE file only archives textures */
	std::vector<string> textures;
	for (int i = 0; i < g_ideFile.size(); i++) {
		textures.push_back(g_ideFile[i].textureArchiveName);
	}
	/* Remove dublicate archive textures */
	remove_duplicates(textures);
	/* Load archive textures (TXD files) */
	for (int i = 0; i < textures.size(); i++) {
		LoadAllTexturesFromTXDFile(imgLoader, textures[i].c_str());
	}


	/* Loading models. IDE file doesn'tcontain dublicate models */
	for (int i = 0; i < g_ideFile.size(); i++) {
		LoadFileDFFWithName(imgLoader, gameRender, g_ideFile[i].modelName.c_str(), g_ideFile[i].objectId);
	}


	/* Load model placement */
	LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ipl");
	LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/bank/bank.ipl");
	

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

			if (gameInput->IsKey(DIK_LSHIFT)) {
				speed *= 50;
			}

			if (gameInput->IsKey(DIK_F1)) {
				gameRender->ChangeRasterizerStateToWireframe();
			}

			if (gameInput->IsKey(DIK_F2)) {
				gameRender->ChangeRasterizerStateToSolid();
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

	for (int i = 0; i < g_LoadedModels.size(); i++) {
		g_LoadedModels[i]->Cleanup();
		delete g_LoadedModels[i];
	}

	delete gameCamera;
	delete gameInput;
	delete gameRender;

	imgLoader->Cleanup();
	delete imgLoader;

	return msg.wParam;
}
