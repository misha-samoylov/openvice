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
#include "Vehicle.h"
#include "loaders/IFP.h"
#include "loaders/COL.hpp"
#include "CollisionWorld.h"
#include "ShadowMap.h"

#define PROJECT_NAME "openvice"
#define WINDOW_WIDTH 3840
#define WINDOW_HEIGHT 2160
#define WINDOW_TITLE L"openvice"
#define CAMERA_FAR_PLANE 1500.0f
#define FOG_START_FACTOR 0.55f
#define FOG_END_FACTOR 0.98f
/* Daytime hour for tobj visibility (re3 CTimeModelInfo). Night lights are off. */
#define WORLD_HOUR 12
/* VC stadium interior IDs (gtamods.com/wiki/Interior). */
#define INTERIOR_DIRTRING 14
#define INTERIOR_BLOODRING 15
#define INTERIOR_HOTRING 16
/* Matches DXRender clear color — fog fades geometry into the sky. */
static const float g_skyColor[4] = { 0.49804f, 0.78431f, 0.94510f, 1.0f };

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
Vehicle* g_vehicle = nullptr;
COL* g_col = nullptr;
CollisionWorld* g_collisionWorld = nullptr;
ShadowMap* g_shadowMap = nullptr;
bool g_controllingVehicle = false;

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

/* GTA VC LODs: "LOD*" / "lod*" prefix, or "IslandLOD*". Case-insensitive. */
static bool IsLodModelName(const char* name)
{
	if (!name)
		return false;
	while (*name == ' ' || *name == '\t')
		name++;
	if (!name[0])
		return false;
	if (_strnicmp(name, "lod", 3) == 0)
		return true;
	if (_strnicmp(name, "IslandLOD", 9) == 0)
		return true;
	return false;
}

/*
 * re3 CTimeModelInfo::FindOtherTimeModel — day/night building pairs use
 * "_dy" / "_nt" suffixes and occupy the same world position in IPL.
 * Some pairs are tobj (timed); a few sit in objs without hours (e.g.
 * miamiland_kb01). Without complementary times both draw → z-fighting
 * (Vice Point Langer = od_bighotel_dy / od_bighotel_nt).
 */
static const char* FindDayNightSuffix(const char* name)
{
	if (!name)
		return nullptr;
	const char* p = strstr(name, "_nt");
	if (p && (p[3] == '\0' || p[3] == '.'))
		return p;
	p = strstr(name, "_dy");
	if (p && (p[3] == '\0' || p[3] == '.'))
		return p;
	return nullptr;
}

static bool IsNightModelName(const char* name)
{
	const char* p = FindDayNightSuffix(name);
	return p && p[1] == 'n';
}

static bool IsDayModelName(const char* name)
{
	const char* p = FindDayNightSuffix(name);
	return p && p[1] == 'd';
}

/* Default VC hotel window: day 5–21, night 21–5 (matches od_bighotel tobj). */
static void EnsureDayNightModelTimes()
{
	int assigned = 0;
	for (size_t i = 0; i < g_models.size(); i++) {
		Model* m = g_models[i];
		if (m->IsTimed())
			continue;
		const char* name = m->GetName().c_str();
		if (IsNightModelName(name)) {
			m->SetTimed(true, 21, 5);
			assigned++;
		} else if (IsDayModelName(name)) {
			m->SetTimed(true, 5, 21);
			assigned++;
		}
	}
	if (assigned > 0)
		printf("[Info] Assigned day/night hours to %d untimed _dy/_nt models\n", assigned);
}

int LoadFileDFFWithName(IMG* pImgLoader, DXRender* render, char *name, int modelId)
{
	/* Skip LOD models — they occupy the same space as HD and cause z-fighting. */
	if (IsLodModelName(name))
		return 0;

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
				 * X � east/west direction
				 * Y � north/south direction
				 * Z � up/down direction
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
				bool isAlpha = g_Textures[matIndex].IsAlpha;
				uint32_t dxt = g_Textures[matIndex].dxtCompression;
				mesh->SetAlpha(isAlpha);
				/* DXT3/4/5 store soft alpha; DXT1 / non-DXT are cutout. */
				mesh->SetAlphaCutout(isAlpha && dxt != 3 && dxt != 4 && dxt != 5);
				
				if (model->IsAlpha() == false && isAlpha) {
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

static bool IsInstanceInShadowRange(Model* model, const SceneInstance& inst,
	float focusX, float focusY, float focusZ, float range)
{
	float cx, cy, cz, radius;
	model->GetWorldCullSphere(
		inst.x, inst.y, inst.z,
		inst.scale[0], inst.scale[1], inst.scale[2],
		&cx, &cy, &cz, &radius
	);

	float dx = cx - focusX;
	float dy = cy - focusY;
	float dz = cz - focusZ;
	float maxR = range + radius;
	return (dx * dx + dy * dy + dz * dz) <= (maxR * maxR);
}

/* alphaFilter: -1 opaque meshes, 0 all, 1 cutout alpha, 2 soft alpha */
static void DrawInstances(DXRender* render, MeshRenderContext& ctx,
	const std::vector<SceneInstance>& instances, int alphaFilter,
	float shadowFocusX = 0.0f, float shadowFocusY = 0.0f, float shadowFocusZ = 0.0f)
{
	const float shadowRange = ShadowMap::CASCADE_HALF_EXTENT;

	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		Model* model = inst.model;

		if (ctx.pass == MESH_PASS_SHADOW) {
			if (!IsInstanceInShadowRange(model, inst, shadowFocusX, shadowFocusY, shadowFocusZ, shadowRange))
				continue;
		} else if (!IsInstanceVisible(model, inst)) {
			continue;
		}

		model->SetPosition(
			inst.x, inst.y, inst.z,
			inst.scale[0], inst.scale[1], inst.scale[2],
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3]
		);
		model->Render(render, ctx, alphaFilter);
	}
}

static void SortAlphaInstancesBackToFront(std::vector<SceneInstance>& instances, Camera* camera)
{
	XMVECTOR camPos = camera->GetPosition();
	float cx = XMVectorGetX(camPos);
	float cy = XMVectorGetY(camPos);
	float cz = XMVectorGetZ(camPos);

	std::sort(instances.begin(), instances.end(),
		[cx, cy, cz](const SceneInstance& a, const SceneInstance& b) {
			float dax = a.x - cx;
			float day = a.y - cy;
			float daz = a.z - cz;
			float dbx = b.x - cx;
			float dby = b.y - cy;
			float dbz = b.z - cz;
			float da = dax * dax + day * day + daz * daz;
			float db = dbx * dbx + dby * dby + dbz * dbz;
			return da > db; /* far first */
		});
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

			/* Skip Hyman Memorial Stadium event arenas (dirt/blood/hotring). */
			if (objectInfo.interior == INTERIOR_DIRTRING
				|| objectInfo.interior == INTERIOR_BLOODRING
				|| objectInfo.interior == INTERIOR_HOTRING)
				continue;

			/* Skip LOD placements even if a model slipped through loading. */
			if (IsLodModelName(objectInfo.modelName))
				continue;

			std::unordered_map<int, Model*>::iterator it = g_modelsById.find(objectInfo.id);
			if (it == g_modelsById.end())
				continue;

			Model* model = it->second;
			/*
			 * Hide timed models outside their hours (re3 GetIsTimeInRange).
			 * Covers tobj nitelites and _dy/_nt building pairs (e.g. Langer).
			 */
			if (!model->IsVisibleAtHour(WORLD_HOUR))
				continue;

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

	printf("[Info] Scene instances: opaque=%u alpha=%u models=%u (hour=%d, timed hidden)\n",
		(unsigned)g_opaqueInstances.size(),
		(unsigned)g_alphaInstances.size(),
		(unsigned)g_models.size(),
		WORLD_HOUR);
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

	/* Shadow cascade follows the camera (free-cam and orbit). */
	XMVECTOR focus = camera->GetPosition();
	float focusX = XMVectorGetX(focus);
	float focusY = XMVectorGetY(focus);
	float focusZ = XMVectorGetZ(focus);

	MeshRenderContext ctx;
	ctx.fogColor = XMFLOAT4(g_skyColor[0], g_skyColor[1], g_skyColor[2], g_skyColor[3]);
	ctx.fogStart = CAMERA_FAR_PLANE * FOG_START_FACTOR;
	ctx.fogEnd = CAMERA_FAR_PLANE * FOG_END_FACTOR;
	ctx.shadowBias = 0.0025f;
	ctx.receiveShadows = 1.0f;

	/* ---- Shadow map pass (casters near focus) ---- */
	if (g_shadowMap) {
		g_shadowMap->UpdateLight(focusX, focusY, focusZ);
		g_shadowMap->Begin(render);

		ctx.pass = MESH_PASS_SHADOW;
		ctx.viewProj = g_shadowMap->GetLightViewProj();
		ctx.lightViewProj = ctx.viewProj;
		ctx.receiveShadows = 0.0f;
		ctx.ClearBindings();

		DrawInstances(render, ctx, g_opaqueInstances, 0, focusX, focusY, focusZ);
		DrawInstances(render, ctx, g_alphaInstances, -1, focusX, focusY, focusZ);
		DrawInstances(render, ctx, g_alphaInstances, 1, focusX, focusY, focusZ);

		if (g_controllingVehicle) {
			if (g_vehicle)
				g_vehicle->Render(render, ctx);
		} else if (g_player) {
			g_player->Render(render, ctx);
		}

		g_shadowMap->End(render);
		ctx.ClearBindings();
	}

	/* ---- Color pass ---- */
	render->RenderStart();

	ctx.pass = MESH_PASS_COLOR;
	ctx.viewProj = XMMatrixMultiply(view, proj);
	ctx.lightViewProj = g_shadowMap ? g_shadowMap->GetLightViewProj() : XMMatrixIdentity();
	ctx.receiveShadows = g_shadowMap ? 1.0f : 0.0f;
	ctx.shadowSRV = g_shadowMap ? g_shadowMap->GetSRV() : nullptr;
	ctx.shadowSampler = g_shadowMap ? g_shadowMap->GetCmpSampler() : nullptr;

	/* Opaque geometry (and opaque submeshes of alpha models). */
	render->SetOpaqueState();
	DrawInstances(render, ctx, g_opaqueInstances, 0);
	DrawInstances(render, ctx, g_alphaInstances, -1);

	if (g_controllingVehicle) {
		if (g_vehicle)
			g_vehicle->Render(render, ctx);
	} else if (g_player) {
		g_player->Render(render, ctx);
	}

	if (g_water)
		g_water->Render(render, camera, g_frustum, CAMERA_FAR_PLANE);

	/* Water changes D3D bindings — reset cache before alpha pass. */
	ctx.ClearBindings();
	ctx.viewProj = XMMatrixMultiply(view, proj);
	ctx.shadowSRV = g_shadowMap ? g_shadowMap->GetSRV() : nullptr;
	ctx.shadowSampler = g_shadowMap ? g_shadowMap->GetCmpSampler() : nullptr;

	SortAlphaInstancesBackToFront(g_alphaInstances, camera);

	/* Cutout first (trees/fences): alpha-test style with depth writes. */
	render->SetCutoutAlphaState();
	DrawInstances(render, ctx, g_alphaInstances, 1);

	/* Soft translucent (glass): back-to-front, no depth write. */
	render->SetSoftAlphaState();
	DrawInstances(render, ctx, g_alphaInstances, 2);

	/* Unbind shadow SRV before Present. */
	if (g_shadowMap) {
		ID3D11ShaderResourceView* nullSRV = nullptr;
		render->GetDeviceContext()->PSSetShaderResources(1, 1, &nullSRV);
	}

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

	g_shadowMap = new ShadowMap();
	if (FAILED(g_shadowMap->Init(render))) {
		printf("[Error] ShadowMap init failed — continuing without dynamic shadows\n");
		g_shadowMap->Cleanup();
		delete g_shadowMap;
		g_shadowMap = nullptr;
	}

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
	int skippedShadows = 0;
	for (int i = 0; i < g_ideFile.size(); i++) {
		for (int j = 0; j < g_ideFile[i]->GetCountItems(); j++) {
			struct itemDefinition* itemDef = &g_ideFile[i]->GetItems()[j];
			/*
			 * IDE flag 0x40 = baked static shadow mesh (tree shadows, etc.).
			 * Skip — replaced by real directional shadow maps.
			 */
			if (itemDef->IsShadowModel()) {
				skippedShadows++;
				continue;
			}

			size_t before = g_models.size();
			LoadFileDFFWithName(imgLoader, render, itemDef->modelName, itemDef->objectId);
			if (g_models.size() > before) {
				Model* loaded = g_models.back();
				loaded->SetTimed(itemDef->isTimed, itemDef->timeOn, itemDef->timeOff);
				/*
				 * Name-based fallback (re3 _dy/_nt pairs). If IDE left the
				 * model untimed (objs section), still give complementary hours.
				 */
				if (!loaded->IsTimed()) {
					if (IsNightModelName(itemDef->modelName))
						loaded->SetTimed(true, 21, 5);
					else if (IsDayModelName(itemDef->modelName))
						loaded->SetTimed(true, 5, 21);
				}
				/*
				 * Drop models that are invisible at WORLD_HOUR (night hotels,
				 * nitelites). Same as re3 not streaming/rendering them.
				 */
				if (!loaded->IsVisibleAtHour(WORLD_HOUR)) {
					loaded->Cleanup();
					delete loaded;
					g_models.pop_back();
				}
			}
		}
	}

	if (skippedShadows > 0)
		printf("[Info] Skipped %d baked shadow models (IDE flag 0x40)\n", skippedShadows);

	/* Catch any remaining untimed _dy/_nt (should be rare after per-item fallback). */
	EnsureDayNightModelTimes();

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
		printf("[Warn] No collision models loaded � player physics limited\n");
	}
	BuildCollisionWorld();

	/* Cheetah = VC model 145 (user's "429" is SA Banshee / VC lamppost). */
	{
		LoadAllTexturesFromTXDFile(imgLoader, "cheetah");
		char cheetahName[] = "cheetah";
		if (LoadFileDFFWithName(imgLoader, render, cheetahName, MI_CHEETAH) == 0) {
			Model* cheetahModel = nullptr;
			for (size_t i = 0; i < g_models.size(); i++) {
				if (g_models[i]->GetId() == MI_CHEETAH) {
					cheetahModel = g_models[i];
					break;
				}
			}
			g_modelsById[MI_CHEETAH] = cheetahModel;
			ColModel* cheetahCol = g_col ? g_col->FindByName("cheetah") : nullptr;
			g_vehicle = new Vehicle();
			if (!g_vehicle->Init(cheetahModel, cheetahCol, g_collisionWorld, imgLoader, render)) {
				printf("[Error] Cheetah vehicle init failed\n");
				g_vehicle->Cleanup();
				delete g_vehicle;
				g_vehicle = nullptr;
			}
		} else {
			printf("[Warn] cheetah.dff not found in IMG\n");
		}
	}

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
			if (g_vehicle) {
				XMVECTOR p = g_player->GetPosition();
				g_vehicle->SetPosition(XMVectorGetX(p) + 4.0f, XMVectorGetY(p) + 2.0f, XMVectorGetZ(p));
				g_vehicle->SetHeading(g_player->GetHeading());
				if (!g_vehicle->PlaceOnGround())
					printf("[Warn] Cheetah PlaceOnGround failed\n");
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
	float freeCamSpeed = 1.0f;
	const float freeCamSpeedMin = 0.1f;
	const float freeCamSpeedMax = 50.0f;
	bool numpad1WasDown = false;
	bool numpad0WasDown = false;
	bool numpad2WasDown = false;

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

			float speed = 10.0f * freeCamSpeed * (float)frameTime;

			if (input->IsKey(DIK_ESCAPE)) {
				PostQuitMessage(EXIT_SUCCESS);
			}

			if (freeCamera && (input->IsKey(DIK_LSHIFT) || input->IsKey(DIK_RSHIFT))) {
				speed *= 5.0f;
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
				bool np2 = input->IsKey(DIK_NUMPAD2);
				if (np1 && !numpad1WasDown && !freeCamera) {
					freeCamera = true;
					XMVECTOR cp = camera->GetPosition();
					camera->SetPosition(XMVectorGetX(cp), XMVectorGetY(cp), XMVectorGetZ(cp));
					printf("[Info] Free camera (NUMPAD1)\n");
				}
				if (np0 && !numpad0WasDown && freeCamera) {
					freeCamera = false;
					XMVECTOR cp = camera->GetPosition();
					float cx = XMVectorGetX(cp);
					float cy = XMVectorGetY(cp);
					float cz = XMVectorGetZ(cp);
					if (g_controllingVehicle && g_vehicle) {
						g_vehicle->SetPosition(cx, cy + 2.5f, cz);
						g_vehicle->PlaceOnGround();
					} else if (g_player) {
						g_player->SetPosition(cx, cy + 1.0f, cz);
						g_player->PlaceOnGround();
					}
					printf("[Info] Follow camera (NUMPAD0) - spawned at free-cam position\n");
				}
				if (np2 && !numpad2WasDown && g_vehicle) {
					g_controllingVehicle = !g_controllingVehicle;
					if (g_controllingVehicle) {
						if (g_player) {
							XMVECTOR p = g_player->GetPosition();
							g_vehicle->SetPosition(
								XMVectorGetX(p), XMVectorGetY(p) + 2.5f, XMVectorGetZ(p));
							g_vehicle->SetHeading(g_player->GetHeading());
							g_vehicle->PlaceOnGround();
						}
						printf("[Info] Driving Cheetah (NUMPAD2) - WASD drive, Space handbrake\n");
					} else {
						if (g_player && g_vehicle) {
							XMVECTOR p = g_vehicle->GetPosition();
							g_player->SetPosition(
								XMVectorGetX(p) + 2.0f, XMVectorGetY(p) + 1.0f, XMVectorGetZ(p));
							g_player->PlaceOnGround();
						}
						printf("[Info] Controlling Tommy (NUMPAD2)\n");
					}
				}
				numpad1WasDown = np1;
				numpad0WasDown = np0;
				numpad2WasDown = np2;
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
					if (freeCamera) {
						/* Scroll up = faster, down = slower. */
						float factor = 1.0f + wheel * 0.0015f;
						if (factor < 0.5f) factor = 0.5f;
						if (factor > 2.0f) factor = 2.0f;
						freeCamSpeed *= factor;
						if (freeCamSpeed < freeCamSpeedMin)
							freeCamSpeed = freeCamSpeedMin;
						if (freeCamSpeed > freeCamSpeedMax)
							freeCamSpeed = freeCamSpeedMax;
					} else {
						camDistance -= wheel * 0.02f;
						if (camDistance < camDistanceMin)
							camDistance = camDistanceMin;
						if (camDistance > camDistanceMax)
							camDistance = camDistanceMax;
					}
				}
			}

			if (!freeCamera && g_controllingVehicle && g_vehicle) {
				float throttle = moveBackForward;
				float steer = -moveLeftRight;
				bool handbrake = input->IsKey(DIK_SPACE);
				g_vehicle->Update((float)frameTime, throttle, steer, handbrake);

				XMVECTOR p = g_vehicle->GetPosition();
				camera->Follow(
					XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p),
					camYaw, camPitch, camDistance, 0.9f
				);
			} else if (g_player && !freeCamera) {
				float s = sinf(camYaw);
				float c = cosf(camYaw);
				float mx = -moveBackForward * s + moveLeftRight * c;
				float mz =  moveBackForward * c + moveLeftRight * s;
				bool moving = (moveLeftRight != 0.0f || moveBackForward != 0.0f);
				/* re3-style: default run, Alt=walk, Shift=sprint (GetSprint). */
				bool walking = moving && (input->IsKey(DIK_LMENU) || input->IsKey(DIK_RMENU));
				bool sprinting = moving && (input->IsKey(DIK_LSHIFT) || input->IsKey(DIK_RSHIFT));
				static bool spaceWasDown = false;
				bool spaceDown = input->IsKey(DIK_SPACE);
				bool jump = spaceDown && !spaceWasDown;
				spaceWasDown = spaceDown;

				g_player->Update((float)frameTime, mx, mz, moving, walking, sprinting, jump);

				XMVECTOR p = g_player->GetPosition();
				camera->Follow(
					XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p),
					camYaw, camPitch, camDistance, 0.95f
				);
			} else {
				if (g_controllingVehicle && g_vehicle)
					g_vehicle->Update((float)frameTime, 0.0f, 0.0f, false);
				else if (g_player)
					g_player->Update((float)frameTime, 0.0f, 0.0f, false, false, false, false);
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
	if (g_vehicle) {
		g_vehicle->Cleanup();
		delete g_vehicle;
		g_vehicle = nullptr;
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

	if (g_shadowMap) {
		g_shadowMap->Cleanup();
		delete g_shadowMap;
		g_shadowMap = nullptr;
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
