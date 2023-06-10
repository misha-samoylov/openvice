#include <string.h>
#include <stdio.h>
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
#include "DXRender.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Window.hpp"
#include "Utils.hpp"
#include "Frustum.h"

#define PROJECT_NAME "openvice"
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE L"openvice"

using namespace DirectX;

/* IPL file contains model position */
struct IPLFile {
	int id;
	char modelName[MAX_LENGTH_FILENAME];
	int interior;
	float posX, posY, posZ;
	float scale[3];
	float rot[4];
};

/* IDE file contains model name and their texture name */
struct IDEFile {
	int objectId;
	char modelName[MAX_LENGTH_FILENAME];
	char textureArchiveName[MAX_LENGTH_FILENAME];
};

struct GameMaterial {
	std::string name; /* without extension ".TXD" */
	uint8_t* source;
	int size;
	uint32_t width;
	uint32_t height;
	uint32_t dxtCompression;
	uint32_t depth;
	bool hasAlpha;
};

struct ModelMaterial {
	std::string materialName;
	int index;
};

std::vector<IDEFile> g_ideFile;
std::vector<IPLFile> g_MapObjects;
std::vector<Mesh*> g_LoadedMeshes;

//std::vector<Mesh*> g_transparentMeshes;
//std::vector<Mesh*> g_notTransparentMeshes;

std::vector<GameMaterial> g_Textures;


template <typename T>
void remove_duplicates(std::vector<T>& vec)
{
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

void LoadAllTexturesFromTXDFile(ImgLoader *pImgLoader, const char *filename)
{
	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, filename);
	strcat(result_name, ".txd");

	int fileId = pImgLoader->GetFileIndexByName(result_name);
	if (fileId == -1) {
		printf("[ERROR] Cannot find file %s in IMG archive\n", result_name);
		return;
	}

	printf("[OK] Finded file %s in IMG archive\n", result_name);

	char *fileBuffer = (char*)pImgLoader->GetFileById(fileId);

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
		m.hasAlpha = t.hasAlpha;

		printf("[OK] Loaded texture name %s from TXD file %s\n", t.name.c_str(), result_name);

		g_Textures.push_back(m);
	}

	//free(fileBuffer);
}

int LoadFileDFFWithName(ImgLoader* pImgLoader, DXRender* render, char *name, int modelId)
{
	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, name);
	strcat(result_name, ".dff");

	int fileId = pImgLoader->GetFileIndexByName(result_name);
	if (fileId == -1) {
		printf("[ERROR] Cannot find %s.dff in IMG archive\n", result_name);
		return 1;
	}

	if (strstr(result_name, "LOD") != NULL) {
		printf("[NOTICE] Skip loading LOD file: %s\n", result_name);
		return 1;
	}

	char* fileBuffer = (char*)pImgLoader->GetFileById(fileId);

	Clump* clump = new Clump();
	clump->Read(fileBuffer);

	for (uint32_t index = 0; index < clump->GetGeometryList().size(); index++) {

		std::vector<ModelMaterial> materIndex;

		//std::vector<float> modelVertices;
		std::vector<float> modelTextureCoord;

		/* Load all materials */
		for (int i = 0; i < clump->GetGeometryList()[index].materialList.size(); i++) {
			Material material = clump->GetGeometryList()[index].materialList[i];

			struct ModelMaterial matInd;
			matInd.materialName = material.texture.name;
			matInd.index = i;
			materIndex.push_back(matInd);
		}

		/*for (uint32_t i = 0; i < clump->GetGeometryList()[index].vertexCount; i++) {

			// Load texture coordinates model
			if (clump->GetGeometryList()[index].flags & FLAGS_TEXTURED
				// || clump->GetGeometryList()[index].flags & FLAGS_TEXTURED2
				) {
				for (uint32_t j = 0; j < 1 / clump->GetGeometryList()[index].numUVs /; j++) { / insert now FLAGS_TEXTURED /

					float tx = clump->GetGeometryList()[index].texCoords[j][i * 2 + 0];
					float ty = clump->GetGeometryList()[index].texCoords[j][i * 2 + 1];

					modelTextureCoord.push_back(tx);
					modelTextureCoord.push_back(ty);
				}
			}
		}*/

		/* Loop for every mesh */
		for (uint32_t i = 0; i < clump->GetGeometryList()[index].splits.size(); i++) {

			uint32_t *meshIndices = clump->GetGeometryList()[index].splits[i].indices;

			/* Save to data for create vertex buffer (x,y,z tx,ty) */
			std::vector<float> meshVertexData;

			for (int v = 0; v < clump->GetGeometryList()[index].vertexCount; v++) {
				float x = clump->GetGeometryList()[index].vertices[v * 3 + 0];
				float y = clump->GetGeometryList()[index].vertices[v * 3 + 1];
				float z = clump->GetGeometryList()[index].vertices[v * 3 + 2];

				float tx = 0.0f;
				float ty = 0.0f;
				if (clump->GetGeometryList()[index].flags & FLAGS_TEXTURED) {
					tx = clump->GetGeometryList()[index].texCoords[0][v * 2 + 0];
					ty = clump->GetGeometryList()[index].texCoords[0][v * 2 + 1];
				}

				/*
				 * Flip coordinates. We use Left Handed Coordinates,
				 * but GTA engine use own coordinate system:
				 * X – east/west direction
				 * Y – north/south direction
				 * Z – up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				meshVertexData.push_back(x);
				meshVertexData.push_back(z);
				meshVertexData.push_back(y);

				meshVertexData.push_back(tx);
				meshVertexData.push_back(ty);
			}

			float* vertices = &meshVertexData[0];
			int countVertices = meshVertexData.size();

			unsigned int* indices = &meshIndices[0]; /* Convert to array unsigned int */
			int countIndices = clump->GetGeometryList()[index].splits[i].m_numIndices; //indices.size();

			D3D_PRIMITIVE_TOPOLOGY topology =
				clump->GetGeometryList()[index].faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			Mesh* mesh = new Mesh();

			printf("Loading mesh\n");
			mesh->Init(render, vertices, countVertices,
				indices, countIndices,
				topology
			);
			printf("Loaded mesh\n");

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
				mesh->SetAlpha(g_Textures[index].hasAlpha); // is transparent or not

				mesh->SetDataDDS(
					render,
					g_Textures[index].source,
					g_Textures[index].size,
					g_Textures[index].width,
					g_Textures[index].height,
					g_Textures[index].dxtCompression,
					g_Textures[index].depth /* TODO: depth is not working */
				);
			}
			mesh->SetId(modelId);

			//if (mesh->GetAlpha() == true)
			//	g_transparentMeshes.push_back(mesh);

			//if (mesh->GetAlpha() == false)
			//	g_notTransparentMeshes.push_back(mesh);

			g_LoadedMeshes.push_back(mesh);
		}
	}

	clump->Clear();
	delete clump;

	return 0;
}

inline float Distance(XMVECTOR v1, XMVECTOR v2)
{
	return XMVectorGetX(XMVector3Length(XMVectorSubtract(v1, v2)));
}
inline float DistanceSquared(XMVECTOR v1, XMVECTOR v2)
{
	return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(v1, v2)));
}


int render_distance = false;

void RenderScene(DXRender *render, Camera *camera)
{
	render->RenderStart();

	float distance = 0;

	Frustum m_frustum;
	m_frustum.ConstructFrustum(1000.0f, camera->GetProjection(), camera->GetView());

	int renderCount = 0;

	/* Рисуем сперва не прозрачные объекты */
	for (int i = 0; i < g_MapObjects.size(); i++) {

		// XMVECTOR cameraPos = camera->GetPosition();
		// XMVECTOR objectPos = XMVectorSet(g_MapObjects[i].posX, g_MapObjects[i].posY, g_MapObjects[i].posZ, 0.0f);
		// distance = Distance(cameraPos, objectPos);

		float x, y, z;
		x = g_MapObjects[i].posX, y = g_MapObjects[i].posY, z = g_MapObjects[i].posZ;
		//m_modellist.GetData(i, x, y, z);

		bool renderModel = m_frustum.CheckSphere(x, y, z, 100.0f);

		if (renderModel) {
			
			//int index = g_MapObjects[i].index;
			//int modelId = g_MapObjects[i].modelId;

		// Получаем местоположения объектов на карте
		//for (int i = 0; i < g_MapObjects.size(); i++) {
			// Проходимся по загруженным моделям

			for (int m = 0; m < g_LoadedMeshes.size(); m++) {

				if (render_distance && distance > 100)
					continue;

				
				if (g_LoadedMeshes[m]->GetAlpha() == true)
					continue;

				int index = i;
				int modelId = g_MapObjects[i].id;

				// Если нашли модель, то ставим ей координаты и рисуем
				if (modelId == g_LoadedMeshes[m]->GetId()) {
					g_LoadedMeshes[m]->SetPosition(
						g_MapObjects[index].posX, g_MapObjects[index].posY, g_MapObjects[index].posZ,
						g_MapObjects[index].scale[0], g_MapObjects[index].scale[1], g_MapObjects[index].scale[2],

						g_MapObjects[index].rot[0], g_MapObjects[index].rot[1], g_MapObjects[index].rot[2], g_MapObjects[index].rot[3]
					);
					g_LoadedMeshes[m]->Render(render, camera);
				}
			}

			renderCount++;
		}
	}


	// Рисуем прозраные объекты
	for (int i = 0; i < g_MapObjects.size(); i++) {

		float x, y, z;
		x = g_MapObjects[i].posX, y = g_MapObjects[i].posY, z = g_MapObjects[i].posZ;
		//m_modellist.GetData(i, x, y, z);

		bool renderModel = m_frustum.CheckSphere(x, y, z, 100.0f);

		if (renderModel) {

			int index = i;
			int modelId = g_MapObjects[i].id;

			// Получаем местоположения объектов на карте
			//for (int i = 0; i < g_MapObjects.size(); i++) {
				// Проходимся по загруженным моделям

			for (int m = 0; m < g_LoadedMeshes.size(); m++) {

				if (render_distance && distance > 100)
					continue;


				if (g_LoadedMeshes[m]->GetAlpha() == false)
					continue;

				// Если нашли модель, то ставим ей координаты и рисуем
				if (modelId == g_LoadedMeshes[m]->GetId()) {

					g_LoadedMeshes[m]->SetPosition(
						g_MapObjects[index].posX, g_MapObjects[index].posY, g_MapObjects[index].posZ,
						g_MapObjects[index].scale[0], g_MapObjects[index].scale[1], g_MapObjects[index].scale[2],

						g_MapObjects[index].rot[0], g_MapObjects[index].rot[1], g_MapObjects[index].rot[2], g_MapObjects[index].rot[3]
					);
					g_LoadedMeshes[m]->Render(render, camera);
				}
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

			if (strcmp(str, "tobj\n") == 0 || strcmp(str, "objs\n")) {
				isObjs = true;
			}

			if (isObjs && strcmp(str, "end\n") == 0) {
				isObjs = false;
			}

			int id = 0;
			char modelName[64];
			char textureArchiveName[64];

			int interior = 0;
			float posX = 0, posY = 0, posZ = 0;
			float scale[3];
			float rot[4];

			int values = sscanf(str, "%d, %64[^,], %64[^,]", &id, modelName, textureArchiveName);

			printf("%s", str);
			if (values == 3 && isObjs) {

				// Добавляем .dff окончание, так как там указано без DFF формата
				//modelNameq = modelNameq + ".dff";

				//std::string mName = modelName;
				//mName = mName + ".dff";

				//std::string taName = textureArchiveName;
				//taName = taName + ".txd";

				struct IDEFile idf;

				idf.objectId = id;
				strcpy(idf.modelName, modelName);
				strcpy(idf.textureArchiveName, textureArchiveName);
				
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

			if (strcmp(str, "end\n") == 0) {
				if (isObjs) {
					isObjs = false;
				}
			}

			int id = 0;
			char modelName[MAX_LENGTH_FILENAME]; // in file without extension (.dff)
			int interior = 0;
			float posX = 0, posY = 0, posZ = 0;
			float scale[3];
			float rot[4];

			int values = sscanf(
				str,
				"%d, %64[^,], %d, "
				"%f, %f, %f, " // pos
				"%f, %f, %f, " // scale
				"%f, %f, %f, %f", // rotation
				&id, modelName, &interior,
				&posX, &posY, &posZ,
				&scale[0], &scale[1], &scale[2],
				&rot[0], &rot[1], &rot[2], &rot[3]
			);

			if (values == 13 && isObjs) {
				printf("%s", str);

				// Добавляем .dff окончание, так как там указано без DFF формата
				//modelNameq = modelNameq + ".dff";

				struct IPLFile iplfile;
				iplfile.id = id;
				strcpy(iplfile.modelName, modelName);
				iplfile.interior = interior;

				/*
				 * Меняем положение модели в пространстве так как наша камера
				 * в Left Handed Coordinates, а движок GTA в своей координатной системе:
				 * X – east/west direction
				 * Y – north/south direction
				 * Z – up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				iplfile.posX = posX;
				iplfile.posY = posZ;
				iplfile.posZ = posY;

				iplfile.scale[0] = scale[0]; // y
				iplfile.scale[1] = scale[2]; // z
				iplfile.scale[2] = scale[1]; // x

				iplfile.rot[0] = rot[0]; // y
				iplfile.rot[1] = rot[2]; // z
				iplfile.rot[2] = rot[1]; // x
				iplfile.rot[3] = rot[3]; // w

				g_MapObjects.push_back(iplfile);
			}
		}
			
	}

	fclose(fp);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, INT nCmdShow)
{
	if (!DirectX::XMVerifyCPUSupport()) {
		MessageBox(NULL, L"You CPU doesn't support DirectXMath.", L"Error", MB_OK);
		return EXIT_FAILURE;
	}

	Window *gameWindow = new Window();
	gameWindow->Init(hInstance, nCmdShow, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

	Input *gameInput = new Input();
	gameInput->Init(hInstance, gameWindow->GetHandleWindow());
	
	Camera *gameCamera = new Camera();
	gameCamera->Init(WINDOW_WIDTH, WINDOW_HEIGHT);
	
	DXRender *gameRender = new DXRender();
	gameRender->Init(gameWindow->GetHandleWindow());

	TCHAR imgPath[] = L"C:/Games/Grand Theft Auto Vice City/models/gta3.img";
	TCHAR dirPath[] = L"C:/Games/Grand Theft Auto Vice City/models/gta3.dir";

	ImgLoader* imgLoader = new ImgLoader();
	imgLoader->Open(
		imgPath,
		dirPath
	);

	/* Load map models and their textures */
	//LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/generic.ide");
	LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ide");
	//LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/bank/bank.ide");
	//LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/downtown/downtown.ide");
	//LoadIDEFile("C:/Games/Grand Theft Auto Vice City/data/maps/littleha/littleha.ide");


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


	/* Loading models. IDE file doesn't contain dublicate models */
	for (int i = 0; i < g_ideFile.size(); i++) {
		LoadFileDFFWithName(imgLoader, gameRender, g_ideFile[i].modelName, g_ideFile[i].objectId);
	}


	/* Load model placement */
	LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/bridge/bridge.ipl");
	//LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/bank/bank.ipl");
	//LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/downtown/downtown.ipl");
	//LoadIPLFile("C:/Games/Grand Theft Auto Vice City/data/maps/littleha/littleha.ipl");
	
	printf("[OK] %s Loaded\n", PROJECT_NAME);

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

			if (Utils::GetTime() > 1.0f) {
				fps = frameCount;
				frameCount = 0;
				Utils::StartTimer();
			}

			frameTime = Utils::GetFrameTime();

			gameInput->Detect();

			float speed = 10.0f * frameTime;

			if (gameInput->IsKey(DIK_ESCAPE)) {
				PostQuitMessage(EXIT_SUCCESS);
			}

			if (gameInput->IsKey(DIK_LSHIFT)) {
				speed *= 50;
			}

			if (gameInput->IsKey(DIK_F1)) {
				gameRender->ChangeRasterizerStateToWireframe();
				printf("Changed render to wireframe\n");
			}

			if (gameInput->IsKey(DIK_F2)) {
				gameRender->ChangeRasterizerStateToSolid();
				printf("Changed render to solid\n");
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

			if (gameInput->IsKey(DIK_NUMPAD0)) {
				render_distance = !render_distance;
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

			RenderScene(gameRender, gameCamera);

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;
		}
	}

	gameRender->Cleanup();
	gameCamera->Cleanup();
	gameInput->Cleanup();

	for (int i = 0; i < g_LoadedMeshes.size(); i++) {
		g_LoadedMeshes[i]->Cleanup();
		delete g_LoadedMeshes[i];
	}

	delete gameCamera;
	delete gameInput;
	delete gameRender;

	imgLoader->Cleanup();
	delete imgLoader;

	return msg.wParam;
}
