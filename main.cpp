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

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE L"openvice"

using namespace DirectX; /* DirectXMath.h */

std::vector<GameModel*> g_Models;

struct GameMaterial {
	std::string name; // без .TXD
	uint8_t *source;
	int size;
	uint32_t width;
	uint32_t height;
	uint32_t dxtCompression;
};

std::vector<GameMaterial> g_Textures;

void LoadAllTexturesFromTXDFile(ImgLoader *pImgLoader, const char *filename)
{
	std::string textureName = filename;
	textureName += ".txd";

	int fileId = pImgLoader->GetFileIndexByName(textureName.c_str());
	if (fileId == -1) {
		printf("[ERROR] Cannot find texture %s in IMG archive\n", textureName);
		return;
	} else {
		printf("[OK] Finded txd file %s in IMG archive\n", textureName);
	}

	char *fileBuffer = pImgLoader->GetFileById(fileId);

	TextureDictionary txd;
	size_t offset = 0;
	txd.read(fileBuffer, &offset);

	// Loop for every texture in TXD file
	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture &t = txd.texList[i];
		cout << i << " " << t.name << " " << t.maskName << " "
			<< " " << t.width[0] << " " << t.height[0] << " "
			<< " " << t.depth << " " << hex << t.rasterFormat << endl;

		//if (txd.texList[i].dxtCompression)
		//	txd.texList[i].decompressDxt();
		//txd.texList[i].convertTo32Bit();

		uint8_t* texelsToArray = txd.texList[i].texels[0];
		size_t len = txd.texList[i].dataSizes[0];

		struct GameMaterial m;
		m.name = t.name; // без .TXD
		m.source = (uint8_t *)malloc(len);
		memcpy(m.source, texelsToArray, len);
		// m.source = *txd.texList[i].texels.data();
		m.size = txd.texList[i].dataSizes[0];
		m.width = txd.texList[i].width[0];
		m.height = txd.texList[i].height[0];
		m.dxtCompression = txd.texList[i].dxtCompression; // DXT1, DXT3, DXT4

		printf("[OK] Loaded texture name %s from TXD file %s\n", t.name, textureName);

		g_Textures.push_back(m);
	}

	free(fileBuffer);
}

int LoadFileDFFWithId(ImgLoader *pImgLoader, GameRender *render, uint32_t fileId, float x = 0, float y = 0, float z = 0)
{
	char* name = pImgLoader->GetFilenameById(fileId);
	if (strstr(name, ".dff") == NULL) {
		printf("You want to load file: %s, but that file is not DFF file\n", name);
		return 0;
	}

	struct materialAndHisIndex {
		std::string materialName;
		int index;
	};

	std::vector<materialAndHisIndex> materIndex;

	char *fileBuffer = pImgLoader->GetFileById(fileId);
	
	Clump *clump = new Clump();
	clump->Read(fileBuffer);
	// clump->Dump();


	for (uint32_t index = 0; index < clump->GetGeometryList().size(); index++) {

		std::vector<float> gvertices;
		std::vector<float> gtexture;

		// «агружаем все материалы
		for (int i = 0; i < clump->GetGeometryList()[index].materialList.size(); i++) {
			Material material = clump->GetGeometryList()[index].materialList[i];

			std::cout << "Model material texture " << material.texture.name << std::endl;

			// —опоставл€ем текстуру и его номер
			struct materialAndHisIndex matInd;
			matInd.materialName = material.texture.name;
			matInd.index = i;
			materIndex.push_back(matInd);

			//LoadTextureWithName(pImgLoader, material.texture.name.c_str(), i);
			
			//for (int iu = 0; iu < g_materials.size(); iu++) {
			//	if (g_materials[iu].name == material.texture.name) {
			//		g_materials[iu].index = iu;
			//	}
			//}

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
				for (uint32_t j = 0; j < 1 /* clump->GetGeometryList()[index].numUVs */; j++) { // вставл€ем пока только  FLAGS_TEXTURED

					float tx = clump->GetGeometryList()[index].texCoords[j][i * 2 + 0]; /* index OR i ??? в последнем [] */
					float ty = clump->GetGeometryList()[index].texCoords[j][i * 2 + 1]; /* index OR i ??? в последнем [] */

					gtexture.push_back(tx);
					gtexture.push_back(ty);
				}
			}
			//else { // делаем загрушку на случай отсутви€ текстуры
				//	gvertices.push_back(0.0);
				//	gvertices.push_back(0.0);
			//}
		}

		/* ѕробегаемс€ по каждой модели	*/
		for (uint32_t i = 0; i < clump->GetGeometryList()[index].splits.size(); i++) {

			std::vector<uint32_t> gindices;

			/**
			 * —охран€ем индексы
			 */
			for (uint32_t j = 0; j < clump->GetGeometryList()[index].splits[i].indices.size(); j++) {
				uint32_t indices = clump->GetGeometryList()[index].splits[i].indices[j];
				gindices.push_back(indices);
			}

			/**
			 * —охран€ем в массив вершины и текстурные координаты,
			 * чтобы создать валидный вершинный буфер
			 */
			std::vector<float> vert;

			for (int v = 0; v < gvertices.size() / 3; v++) {
				float x = gvertices[v * 3 + 0];
				float y = gvertices[v * 3 + 1];
				float z = gvertices[v * 3 + 2];

				float tx = gtexture[v * 2 + 0];
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

			GameModel* gameModel = new GameModel();
			
			gameModel->Init(render, vertices, countVertices,
				indices, countIndices,
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 
				x, y, z
			);

			uint32_t materialIndex = clump->GetGeometryList()[index].splits[i].matIndex;

			int index = -1;

			// получаем номер текстуры по его имени
			for (int ib = 0; ib < materIndex.size(); ib++) {
				for (int im = 0; im < g_Textures.size(); im++) {
					if (g_Textures[im].name == materIndex[ib].materialName) {
						index = im;
						break;
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
					g_Textures[index].dxtCompression
				);
			}

			g_Models.push_back(gameModel);

			gameModel->SetPosition(x, y, z);
		}
	}

	clump->Clear();
	delete clump;

	free(fileBuffer);
}


void LoadFileDFFWithName(ImgLoader* pImgLoader, GameRender* render, std::string name, float x, float y, float z)
{
	int index = pImgLoader->GetFileIndexByName(name.c_str());
	if (index == -1) {
		printf("Cannot find %s.dff in IMG archive\n", name);
		return;
	}
	else {
		printf("Finded dff file %s.dff in IMG archive\n", name);
	}

	LoadFileDFFWithId(pImgLoader, render, index, x, y, z);
}

void Render(GameRender *render, GameCamera *camera)
{
	render->RenderStart();

	for (int i = 0; i < g_Models.size(); i++) {
		g_Models[i]->Render(render, camera);
	}

	render->RenderEnd();
}

// IDE файлы содержат название модели и еЄ архива текстур (TXD)

struct IDEFile {
	int objectId;
	std::string modelName;
	std::string textureArchiveName;
};

std::vector<IDEFile> ideFile;

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
				
				ideFile.push_back(idf);
			}
		}

	}

	fclose(fp);
}

struct IPLFile {
	int id;
	std::string modelName;
	int interior;
	float posX, posY, posZ;
	float scale[3];
	float rot[4];
};

std::vector<IPLFile> objects;

// IPL файлы содержат местоположение модели
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
				iplfile.posX = posX;
				iplfile.posY = posY;
				iplfile.posZ = posZ;
				objects.push_back(iplfile);
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

	// «агрузка сопоставление модели и еЄ текстуры
	LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ide");

	for (int i = 0; i < ideFile.size(); i++) {
		LoadAllTexturesFromTXDFile(imgLoader, ideFile[i].textureArchiveName.c_str());
	}

	// «агрузка местоположени€ модели
	LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ipl");


	for (int i = 0; i < objects.size(); i++) {
		LoadFileDFFWithName(imgLoader, gameRender, objects[i].modelName.c_str(),
			objects[i].posX, objects[i].posY, objects[i].posZ);
	}

	

	//gm = LoadFileDFFWithId(imgLoader, gameRender, 189);
	//gm->SetPosition(10, 10, 10);

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

	for (int i = 0; i < g_Models.size(); i++) {
		g_Models[i]->Cleanup();
		delete g_Models[i];
	}

	delete gameCamera;
	delete gameInput;
	delete gameRender;

	imgLoader->Cleanup();
	delete imgLoader;

	return msg.wParam;
}