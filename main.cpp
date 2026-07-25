#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <float.h>
#include <cmath>
#include <unordered_map>

#include <windows.h>

#include "renderware.h"
#include "loaders/Clump.h"
#include "loaders/IMG.hpp"
#include "loaders/IPL.hpp"
#include "loaders/IDE.hpp"
#include "Mesh.hpp"
#include "DXRender.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Window.hpp"
#include "Utils.hpp"
#include "Frustum.h"
#include "Model.h"
#include "Water.h"
#include "Player.h"
#include "loaders/IFP.h"
#include "loaders/COL.hpp"
#include "CollisionWorld.h"

#define PROJECT_NAME "openvice"
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE L"openvice"
#define CAMERA_FAR_PLANE 1000.0f

int frameCount = 0;
Frustum g_frustum;

struct GameMaterial {
	char name[MAX_LENGTH_FILENAME]; /* without extension ".TXD" */
	uint8_t* source;
	int size;
	uint32_t width;
	uint32_t height;
	uint32_t dxtCompression;
	uint32_t depth;
	bool IsAlpha;
};

struct ModelMaterial {
	char materialName[MAX_LENGTH_FILENAME];
	int index;
};

std::vector<Model*> g_models;
std::vector<IDE*> g_ideFile;
std::vector<GameMaterial> g_Textures;
std::vector<IPL*> g_ipl;
std::unordered_map<int, Model*> g_modelsById;

struct SceneInstance {
	Model* model;
	float x, y, z;
	float scale[3];
	float rotation[4];
};

std::vector<SceneInstance> g_opaqueInstances;
std::vector<SceneInstance> g_alphaInstances;
Water* g_water = nullptr;
Player* g_player = nullptr;
COL* g_col = nullptr;
CollisionWorld* g_collisionWorld = nullptr;

template <typename T>
void remove_duplicates(std::vector<T>& vec)
{
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

void LoadAllTexturesFromTXDFile(IMG *pImgLoader, const char *filename)
{
	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, filename);
	strcat(result_name, ".txd");

	int fileId = pImgLoader->GetFileIndexByName(result_name);
	if (fileId == -1) {
		// printf("[Error] Cannot find file %s in IMG archive\n", result_name);
		return;
	}

	// printf("[Info] Loading file %s from IMG archive\n", filename);

	char *fileBuffer = (char*)pImgLoader->GetFileById(fileId);

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);

	/* Loop for every texture in TXD file */
	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture &t = txd.texList[i];
		// printf("%s %s %d %d %d %d\n", t.name, t.maskName.c_str(), t.width[0], t.height[0], t.depth, t.rasterFormat);
		
		uint8_t* texelsToArray = t.texels[0];
		size_t len = t.dataSizes[0];

		struct GameMaterial m;
		memcpy(m.name, t.name, sizeof(t.name)); /* without extension ".TXD" */

		/* TODO: Replace copy to buffer to best solution */
		/* TODO: Free memory */
		m.source = (uint8_t *)malloc(len);
		memcpy(m.source, texelsToArray, len);

		m.size = t.dataSizes[0];
		m.width = t.width[0];
		m.height = t.height[0];
		m.dxtCompression = t.dxtCompression; /* DXT1, DXT3, DXT4 */
		m.depth = t.depth;
		m.IsAlpha = t.IsAlpha;

		// printf("[OK] Loaded texture name %s from TXD file %s\n", t.name, result_name);

		g_Textures.push_back(m);
	}

	//free(fileBuffer);
}

int LoadFileDFFWithName(IMG* pImgLoader, DXRender* render, char *name, int modelId)
{
	/* Skip LOD files */
	if (strstr(name, "LOD") != NULL) {
		return 0;
	}

	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, name);
	strcat(result_name, ".dff");

	int fileId = pImgLoader->GetFileIndexByName(result_name);
	if (fileId == -1) {
		return 1;
	}

	char* fileBuffer = (char*)pImgLoader->GetFileById(fileId);

	Clump* clump = new Clump();
	clump->Read(fileBuffer);

	Model* model = new Model();
	model->SetId(modelId);
	model->SetName(name);
	model->SetAlpha(false);

	for (uint32_t index = 0; index < clump->m_numGeometries; index++) {

		Geometry* geometry = clump->GetGeometryList()[index];
		std::vector<ModelMaterial> materIndex;

		/* Load all materials */
		uint32_t materials = geometry->m_numMaterials;
		for (uint32_t i = 0; i < materials; i++) {
			Material *material = geometry->materialList[i];

			struct ModelMaterial matInd;
			std::string b = material->texture.name;
			//matInd.materialName = b;
			memcpy(matInd.materialName, b.c_str(), sizeof(matInd.materialName));
			matInd.index = i;

			materIndex.push_back(matInd);
		}

		/*
		 * Local cull sphere in engine coords (same Y/Z remap as vertices).
		 * Prefer the DFF morph-target sphere, then expand from vertex AABB
		 * so large/offset geometry is never under-bounded.
		 */
		{
			float* bs = geometry->boundingSphere;
			model->IncludeBoundingSphere(bs[0], bs[2], bs[1], bs[3]);

			if (geometry->vertices != NULL && geometry->vertexCount > 0) {
				float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
				float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

				for (uint32_t v = 0; v < geometry->vertexCount; v++) {
					float vx = geometry->vertices[v * 3 + 0];
					float vy = geometry->vertices[v * 3 + 2];
					float vz = geometry->vertices[v * 3 + 1];

					if (vx < minX) minX = vx;
					if (vy < minY) minY = vy;
					if (vz < minZ) minZ = vz;
					if (vx > maxX) maxX = vx;
					if (vy > maxY) maxY = vy;
					if (vz > maxZ) maxZ = vz;
				}

				float cx = (minX + maxX) * 0.5f;
				float cy = (minY + maxY) * 0.5f;
				float cz = (minZ + maxZ) * 0.5f;
				float hx = (maxX - minX) * 0.5f;
				float hy = (maxY - minY) * 0.5f;
				float hz = (maxZ - minZ) * 0.5f;
				float radius = sqrtf(hx * hx + hy * hy + hz * hz);
				model->IncludeBoundingSphere(cx, cy, cz, radius);
			}
		}

		/* Loop for every mesh */
		for (uint32_t i = 0; i < geometry->splits.size(); i++) {

			int v_count = geometry->vertexCount;

			/* Save to data for create vertex buffer (x,y,z tx,ty) */
			// TODO: Free memory
			float *meshVertexData = (float*)malloc(sizeof(float) * v_count * 5);

			for (int v = 0; v < v_count; v++) {
				float x = geometry->vertices[v * 3 + 0];
				float y = geometry->vertices[v * 3 + 1];
				float z = geometry->vertices[v * 3 + 2];

				float tx = 0.0f;
				float ty = 0.0f;
				if (geometry->flags & FLAGS_TEXTURED) {
					tx = geometry->texCoords[0][v * 2 + 0];
					ty = geometry->texCoords[0][v * 2 + 1];
				}

				/*
				 * Flip coordinates. We use Left Handed Coordinates,
				 * but GTA engine use own coordinate system:
				 * X  east/west direction
				 * Y  north/south direction
				 * Z  up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				meshVertexData[v * 5 + 0] = x;
				meshVertexData[v * 5 + 1] = z;
				meshVertexData[v * 5 + 2] = y;

				meshVertexData[v * 5 + 3] = tx;
				meshVertexData[v * 5 + 4] = ty;
			}

			D3D_PRIMITIVE_TOPOLOGY topology =
				geometry->faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			Mesh* mesh = new Mesh();

			mesh->Init(
				render, 
				meshVertexData,
				v_count * 5,
				(unsigned int*)geometry->splits[i].indices,
				geometry->splits[i].m_numIndices,
				topology
			);

			uint32_t materialIndex = geometry->splits[i].matIndex;

			int matIndex = -1;

			// Find texture by index
			for (int ib = 0; ib < materIndex.size(); ib++) {

				if (materialIndex == materIndex[ib].index) {

					for (int im = 0; im < g_Textures.size(); im++) {
						if (strcmp(g_Textures[im].name, materIndex[ib].materialName) == 0) {
							matIndex = im;
							break;
						}
					}

				}
			}

			if (matIndex != -1) {
				mesh->SetAlpha(g_Textures[matIndex].IsAlpha);
				
				if (model->IsAlpha() == false && g_Textures[matIndex].IsAlpha) {
					model->SetAlpha(true);
				}

				mesh->SetDataDDS(
					render,
					g_Textures[matIndex].source,
					g_Textures[matIndex].size,
					g_Textures[matIndex].width,
					g_Textures[matIndex].height,
					g_Textures[matIndex].dxtCompression,
					g_Textures[matIndex].depth
				);
			}

			model->AddMesh(mesh);
		}
	}

	clump->Clear();
	delete clump;

	g_models.push_back(model);

	return 0;
}

static bool IsInstanceVisible(Model* model, const SceneInstance& inst)
{
	float cx, cy, cz, radius;
	model->GetWorldCullSphere(
		inst.x, inst.y, inst.z,
		inst.scale[0], inst.scale[1], inst.scale[2],
		&cx, &cy, &cz, &radius
	);

	return g_frustum.CheckSphere(cx, cy, cz, radius);
}

static void DrawInstances(DXRender* render, MeshRenderContext& ctx, const std::vector<SceneInstance>& instances)
{
	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		Model* model = inst.model;

		if (!IsInstanceVisible(model, inst))
			continue;

		model->SetPosition(
			inst.x, inst.y, inst.z,
			inst.scale[0], inst.scale[1], inst.scale[2],
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3]
		);
		model->Render(render, ctx);
	}
}

void BuildSceneInstances()
{
	g_modelsById.clear();
	g_opaqueInstances.clear();
	g_alphaInstances.clear();

	for (size_t i = 0; i < g_models.size(); i++) {
		g_modelsById[g_models[i]->GetId()] = g_models[i];
	}

	g_opaqueInstances.reserve(65536);
	g_alphaInstances.reserve(8192);

	for (size_t i = 0; i < g_ipl.size(); i++) {
		int count = g_ipl[i]->GetCountObjects();
		for (int j = 0; j < count; j++) {
			mapItem objectInfo = g_ipl[i]->GetItem(j);

			std::unordered_map<int, Model*>::iterator it = g_modelsById.find(objectInfo.id);
			if (it == g_modelsById.end())
				continue;

			Model* model = it->second;
			SceneInstance inst;
			inst.model = model;
			inst.x = objectInfo.x;
			inst.y = objectInfo.y;
			inst.z = objectInfo.z;
			inst.scale[0] = objectInfo.scale[0];
			inst.scale[1] = objectInfo.scale[1];
			inst.scale[2] = objectInfo.scale[2];
			inst.rotation[0] = objectInfo.rotation[0];
			inst.rotation[1] = objectInfo.rotation[1];
			inst.rotation[2] = objectInfo.rotation[2];
			inst.rotation[3] = objectInfo.rotation[3];

			if (model->IsAlpha())
				g_alphaInstances.push_back(inst);
			else
				g_opaqueInstances.push_back(inst);
		}
	}

	printf("[Info] Scene instances: opaque=%u alpha=%u models=%u\n",
		(unsigned)g_opaqueInstances.size(),
		(unsigned)g_alphaInstances.size(),
		(unsigned)g_models.size());
}

static void AppendColPlacements(const std::vector<SceneInstance>& instances, std::vector<ColInstancePlacement>& out)
{
	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		if (!inst.model || !g_col)
			continue;
		ColModel* col = g_col->FindByName(inst.model->GetName().c_str());
		if (!col)
			continue;

		ColInstancePlacement p;
		p.model = col;
		p.x = inst.x;
		p.y = inst.y;
		p.z = inst.z;
		p.scale[0] = inst.scale[0];
		p.scale[1] = inst.scale[1];
		p.scale[2] = inst.scale[2];
		p.rotation[0] = inst.rotation[0];
		p.rotation[1] = inst.rotation[1];
		p.rotation[2] = inst.rotation[2];
		p.rotation[3] = inst.rotation[3];
		out.push_back(p);
	}
}

void BuildCollisionWorld()
{
	if (g_collisionWorld) {
		g_collisionWorld->Clear();
		delete g_collisionWorld;
		g_collisionWorld = nullptr;
	}
	if (!g_col)
		return;

	std::vector<ColInstancePlacement> placements;
	placements.reserve(g_opaqueInstances.size() + g_alphaInstances.size());
	AppendColPlacements(g_opaqueInstances, placements);
	AppendColPlacements(g_alphaInstances, placements);

	g_collisionWorld = new CollisionWorld();
	g_collisionWorld->Build(g_col, placements);
}

void RenderScene(DXRender *render, Camera *camera)
{
	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	g_frustum.ConstructFrustum(CAMERA_FAR_PLANE, proj, view);

	render->RenderStart();

	MeshRenderContext ctx;
	ctx.viewProj = XMMatrixMultiply(view, proj);

	DrawInstances(render, ctx, g_opaqueInstances);

	if (g_player)
		g_player->Render(render, ctx);

	if (g_water)
		g_water->Render(render, camera, g_frustum, CAMERA_FAR_PLANE);

	/* Water changes D3D bindings  reset cache before alpha pass. */
	ctx = MeshRenderContext();
	ctx.viewProj = XMMatrixMultiply(view, proj);

	DrawInstances(render, ctx, g_alphaInstances);

	render->RenderEnd();
}

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	bool vsync = false;

	if (!DirectX::XMVerifyCPUSupport()) {
		MessageBox(NULL, L"You CPU doesn't support DirectXMath", L"Error", MB_OK);
		return 1;
	}

	Window* window = new Window();
	window->Init(hInstance, nCmdShow, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

	Input* input = new Input();
	input->Init(hInstance, window->GetHandleWindow());

	Camera* camera = new Camera();
	camera->Init(WINDOW_WIDTH, WINDOW_HEIGHT, CAMERA_FAR_PLANE);

	DXRender* render = new DXRender();
	render->Init(window->GetHandleWindow(), vsync);

	TCHAR imgPath[] = L"C:/Games/Grand Theft Auto Vice City/models/gta3.img";
	TCHAR dirPath[] = L"C:/Games/Grand Theft Auto Vice City/models/gta3.dir";

	IMG* imgLoader = new IMG();
	imgLoader->Open(imgPath, dirPath);

	char maps[][MAX_LENGTH_FILENAME] = {
		{ "airport" },
		{ "airportN" },
		{ "bank" },
		{ "bar" },
		{ "bridge" },
		{ "cisland" },
		{ "club" },
		{ "concerth"},
		{ "docks"},
		{ "downtown"},
		{ "downtows"},
		{ "golf" },
		{ "haiti" },
		{ "haitiN" },
		{ "hotel" },
		{ "islandsf" },
		{ "lawyers" },
		{ "littleha" },
		{ "mall" },
		{ "mansion" },
		{ "nbeach" },
		{ "nbeachbt" },
		{ "nbeachw" },
		{ "oceandn" },
		{ "oceandrv" },
		{ "stadint" },
		{ "starisl" },
		{ "stripclb" },
		{ "washintn" },
		{ "washints" },
		{ "yacht" }
	};

	/* Load map models and their textures */
	for (int i = 0; i < sizeof(maps) / sizeof(maps[0]); i++) {
		IDE* ide = new IDE();

		char path[256];
		strcpy(path, "C:/Games/Grand Theft Auto Vice City/data/maps/");
		strcat(path, maps[i]);
		strcat(path, "/");
		strcat(path, maps[i]);
		strcat(path, ".ide");

		int res = ide->Load(path);

		assert(res == 0);

		g_ideFile.push_back(ide);
	}

	IDE* ide = new IDE();
	int res = ide->Load("C:/Games/Grand Theft Auto Vice City/data/maps/generic.ide");
	assert(res == 0);
	g_ideFile.push_back(ide);

	/* Load from IDE file only archives textures */
	std::vector<string> textures;
	for (int i = 0; i < g_ideFile.size(); i++) {
		int count = g_ideFile[i]->GetCountItems();

		for (int j = 0; j < count; j++) {
			struct itemDefinition* item = &g_ideFile[i]->GetItems()[j];
			textures.push_back(item->textureArchiveName);
		}
	}

	/* Remove dublicate archive textures */
	remove_duplicates(textures);

	/* Load archive textures (TXD files) */
	for (int i = 0; i < textures.size(); i++) {
		LoadAllTexturesFromTXDFile(imgLoader, textures[i].c_str());
	}


	/* Loading models. IDE file doesn't contain dublicate models */
	for (int i = 0; i < g_ideFile.size(); i++) {
		for (int j = 0; j < g_ideFile[i]->GetCountItems(); j++) {
			struct itemDefinition* itemDef = &g_ideFile[i]->GetItems()[j];
			LoadFileDFFWithName(imgLoader, render, itemDef->modelName, itemDef->objectId);
		}
	}

	for (int i = 0; i < sizeof(maps) / sizeof(maps[0]); i++) {
		char path[256];
		strcpy(path, "C:/Games/Grand Theft Auto Vice City/data/maps/");
		strcat(path, maps[i]);
		strcat(path, "/");
		strcat(path, maps[i]);
		strcat(path, ".ipl");

		IPL* ipl = new IPL();
		ipl->Load(path);
		g_ipl.push_back(ipl);
	}

	BuildSceneInstances();

	g_col = new COL();
	if (!g_col->LoadAllFromIMG(imgLoader)) {
		printf("[Warn] No collision models loaded  player physics limited\n");
	}
	BuildCollisionWorld();

	g_water = new Water();
	if (!g_water->Init(
		render,
		"C:/Games/Grand Theft Auto Vice City/data/waterpro.dat",
		"C:/Games/Grand Theft Auto Vice City/models/particle.txd"
	)) {
		printf("[Warn] Water failed to init, continuing without water\n");
		delete g_water;
		g_water = nullptr;
	}

	IFP* ifp = new IFP();
	if (!ifp->Load("C:/Games/Grand Theft Auto Vice City/anim/ped.ifp")) {
		printf("[Error] Failed to load ped.ifp - player disabled\n");
		delete ifp;
		ifp = nullptr;
	} else {
		g_player = new Player();
		if (!g_player->Init(imgLoader, render, ifp)) {
			printf("[Error] Player init failed\n");
			g_player->Cleanup();
			delete g_player;
			g_player = nullptr;
		} else {
			g_player->SetCollisionWorld(g_collisionWorld);
			/* Ocean Drive-ish spawn; PlaceOnGround snaps via COL like re3 FindZCoorForPed. */
			g_player->SetPosition(0.0f, 50.0f, 0.0f);
			if (!g_player->PlaceOnGround()) {
				printf("[Warn] Player PlaceOnGround failed at spawn, keeping Y=50\n");
			} else {
				XMVECTOR p = g_player->GetPosition();
				printf("[Info] Player placed on ground at %.2f %.2f %.2f\n",
					XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p));
			}
		}
	}

	printf("[Info] %s loaded\n", PROJECT_NAME);

	float moveLeftRight = 0.0f;
	float moveBackForward = 0.0f;

	float camYaw = 0.0f;
	float camPitch = 0.25f;
	float camDistance = 14.0f;
	const float camDistanceMin = 3.0f;
	const float camDistanceMax = 40.0f;
	bool freeCamera = false;
	bool numpad1WasDown = false;
	bool numpad0WasDown = false;

	DIMOUSESTATE mouseLastState;
	DIMOUSESTATE mouseCurrState;

	mouseCurrState.lX = input->GetMouseSpeedX();
	mouseCurrState.lY = input->GetMouseSpeedY();

	mouseLastState.lX = input->GetMouseSpeedX();
	mouseLastState.lY = input->GetMouseSpeedY();

	
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

			input->Detect();

			float speed = 10.0f * (float)frameTime;

			if (input->IsKey(DIK_ESCAPE)) {
				PostQuitMessage(EXIT_SUCCESS);
			}

			if (input->IsKey(DIK_LSHIFT)) {
				speed *= 50;
			}

			if (input->IsKey(DIK_F1)) {
				render->ChangeRasterizerStateToWireframe();
				printf("[Info] Changed render to wireframe\n");
			}

			if (input->IsKey(DIK_F2)) {
				render->ChangeRasterizerStateToSolid();
				printf("[Info] Changed render to solid\n");
			}

			{
				bool np1 = input->IsKey(DIK_NUMPAD1);
				bool np0 = input->IsKey(DIK_NUMPAD0);
				if (np1 && !numpad1WasDown && !freeCamera) {
					freeCamera = true;
					XMVECTOR cp = camera->GetPosition();
					camera->SetPosition(XMVectorGetX(cp), XMVectorGetY(cp), XMVectorGetZ(cp));
					printf("[Info] Free camera (NUMPAD1)  WASD fly, Shift boost\n");
				}
				if (np0 && !numpad0WasDown && freeCamera) {
					freeCamera = false;
					printf("[Info] Follow camera (NUMPAD0)  locked to Tommy\n");
				}
				numpad1WasDown = np1;
				numpad0WasDown = np0;
			}

			moveLeftRight = 0.0f;
			moveBackForward = 0.0f;

			if (input->IsKey(DIK_W)) {
				moveBackForward += 1.0f;
			}

			if (input->IsKey(DIK_A)) {
				moveLeftRight -= 1.0f;
			}

			if (input->IsKey(DIK_S)) {
				moveBackForward -= 1.0f;
			}

			if (input->IsKey(DIK_D)) {
				moveLeftRight += 1.0f;
			}

			mouseCurrState.lX = input->GetMouseSpeedX();
			mouseCurrState.lY = input->GetMouseSpeedY();

			if ((mouseCurrState.lX != mouseLastState.lX)
				|| (mouseCurrState.lY != mouseLastState.lY)) {

				if (freeCamera) {
					/* Free look: mouse direction = look direction. */
					camYaw += mouseCurrState.lX * 0.001f;
					camPitch += mouseCurrState.lY * 0.001f;
					if (camPitch > 1.55f) camPitch = 1.55f;
					if (camPitch < -1.55f) camPitch = -1.55f;
				} else {
					/* Orbit follow: mouse right looks right; Y inverted
					 * (mouse down raises camera / looks down on Tommy). */
					camYaw -= mouseCurrState.lX * 0.001f;
					camPitch -= mouseCurrState.lY * 0.001f;
				}

				mouseLastState = mouseCurrState;
			}

			{
				float wheel = input->GetMouseWheel();
				if (wheel != 0.0f) {
					camDistance -= wheel * 0.02f;
					if (camDistance < camDistanceMin)
						camDistance = camDistanceMin;
					if (camDistance > camDistanceMax)
						camDistance = camDistanceMax;
				}
			}

			if (g_player && !freeCamera) {
				/*
				 * Camera::Follow places the camera at
				 *   (sy*cp, -sp, -cy*cp) * distance from the player.
				 * Horizontal look / walk direction is therefore (-sy, cy) on XZ,
				 * and camera right is (cy, sy)  same as GTA VC 3rd-person.
				 */
				float s = sinf(camYaw);
				float c = cosf(camYaw);
				float mx = -moveBackForward * s + moveLeftRight * c;
				float mz =  moveBackForward * c + moveLeftRight * s;
				bool moving = (moveLeftRight != 0.0f || moveBackForward != 0.0f);
				bool running = moving && input->IsKey(DIK_LSHIFT);
				/* Edge-trigger Space like re3 JumpJustDown. */
				static bool spaceWasDown = false;
				bool spaceDown = input->IsKey(DIK_SPACE);
				bool jump = spaceDown && !spaceWasDown;
				spaceWasDown = spaceDown;

				g_player->Update((float)frameTime, mx, mz, moving, running, jump);

				XMVECTOR p = g_player->GetPosition();
				camera->Follow(
					XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p),
					camYaw, camPitch, camDistance, 1.4f
				);
			} else {
				/* Free camera: WASD flies; Tommy stays put (idle). */
				if (g_player)
					g_player->Update((float)frameTime, 0.0f, 0.0f, false, false, false);
				camera->Update(camPitch, camYaw, moveLeftRight * speed, moveBackForward * speed);
			}

			if (g_water)
				g_water->Update((float)frameTime);

			RenderScene(render, camera);
		}
	}

	if (g_player) {
		g_player->Cleanup();
		delete g_player;
		g_player = nullptr;
	}
	if (ifp) {
		ifp->Cleanup();
		delete ifp;
		ifp = nullptr;
	}

	if (g_collisionWorld) {
		g_collisionWorld->Clear();
		delete g_collisionWorld;
		g_collisionWorld = nullptr;
	}
	if (g_col) {
		g_col->Cleanup();
		delete g_col;
		g_col = nullptr;
	}

	if (g_water) {
		g_water->Cleanup();
		delete g_water;
		g_water = nullptr;
	}

	render->Cleanup();
	camera->Cleanup();
	input->Cleanup();

	for (int i = 0; i < g_ipl.size(); i++) {
		g_ideFile[i]->Cleanup();
		delete g_ideFile[i];
	}

	for (int i = 0; i < g_ipl.size(); i++) {
		g_ipl[i]->Cleanup();
		delete g_ipl[i];
	}

	for (int i = 0; i < g_models.size(); i++) {
		g_models[i]->Cleanup();
		delete g_models[i];
	}

	delete camera;
	delete input;
	delete render;

	imgLoader->Cleanup();
	delete imgLoader;

	return msg.wParam;
}
