#include "world/GameWorld.h"
#include "assets/DffLoader.h"
#include "assets/ContentLoader.h"
#include "core/GameConfig.h"
#include "PhysicsDebugDraw.h"

#include <stdio.h>

void GameWorld::AppendColPlacements(
	COL* col,
	const std::vector<SceneInstance>& instances,
	std::vector<ColInstancePlacement>& out)
{
	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		if (!inst.model || !col)
			continue;
		ColModel* cm = col->FindByName(inst.model->GetName().c_str());
		if (!cm)
			continue;

		ColInstancePlacement p;
		p.model = cm;
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

void GameWorld::BuildCollision(AssetRegistry& assets, Scene& scene, PhysicsDebugDraw* debugDraw)
{
	m_collision.reset();
	COL* col = assets.Col();
	if (!col)
		return;

	std::vector<ColInstancePlacement> placements;
	placements.reserve(scene.Opaque().size() + scene.Alpha().size());
	AppendColPlacements(col, scene.Opaque(), placements);
	AppendColPlacements(col, scene.Alpha(), placements);

	m_collision.reset(new CollisionWorld());
	m_collision->Build(col, placements);
	if (debugDraw)
		m_collision->SetDebugDrawer(debugDraw);
}

bool GameWorld::InitFromAssets(
	AssetRegistry& assets,
	Scene& scene,
	IMG* img,
	DXRender* render,
	PhysicsDebugDraw* debugDraw)
{
	m_controllingVehicle = false;

	std::unique_ptr<COL> col(new COL());
	if (!col->LoadAllFromIMG(img)) {
		printf("[Warn] No collision models loaded — player physics limited\n");
	}
	assets.SetCol(std::move(col));

	BuildCollision(assets, scene, debugDraw);

	ContentLoader::LoadTexturesFromTxd(img, assets, "cheetah");
	char cheetahName[] = "cheetah";
	assets.Txd().PushCurrentTxd();
	assets.Txd().SetCurrentTxdByName("cheetah");
	int cheetahLoad = DffLoader::LoadVehicleDff(img, render, assets, cheetahName, MI_CHEETAH);
	assets.Txd().PopCurrentTxd();
	if (cheetahLoad == 0) {
		Model* cheetahModel = assets.FindModelById(MI_CHEETAH);
		ColModel* cheetahCol = assets.Col() ? assets.Col()->FindByName("cheetah") : nullptr;
		m_vehicle.reset(new Vehicle());
		if (!m_vehicle->Init(cheetahModel, cheetahCol, m_collision.get(), img, render)) {
			printf("[Error] Cheetah vehicle init failed\n");
			m_vehicle->Cleanup();
			m_vehicle.reset();
		}
	} else {
		printf("[Warn] cheetah.dff not found in IMG\n");
	}

	m_water.reset(new Water());
	if (!m_water->Init(render, GTA_VC_WATERPRO, GTA_VC_PARTICLE_TXD)) {
		printf("[Warn] Water failed to init, continuing without water\n");
		m_water.reset();
	}

	m_clouds.reset(new Clouds());
	if (!m_clouds->Init(render, GTA_VC_PARTICLE_TXD)) {
		printf("[Warn] Clouds failed to init, continuing without clouds\n");
		m_clouds->Cleanup();
		m_clouds.reset();
	}

	m_ifp.reset(new IFP());
	if (!m_ifp->Load(GTA_VC_PED_IFP)) {
		printf("[Error] Failed to load ped.ifp - player disabled\n");
		m_ifp.reset();
	} else {
		m_player.reset(new Player());
		if (!m_player->Init(img, render, m_ifp.get())) {
			printf("[Error] Player init failed\n");
			m_player->Cleanup();
			m_player.reset();
		} else {
			m_player->SetCollisionWorld(m_collision.get());
			float sx = 0.0f, sy = 50.0f, sz = 0.0f;
			if (scene.GetSpawnHint(&sx, &sy, &sz)) {
				printf("[Info] Spawn hint from scene: %.1f %.1f %.1f\n", sx, sy, sz);
			} else {
				printf("[Warn] No scene spawn hint — using origin\n");
			}
			m_player->SetPosition(sx, sy, sz);
			if (!m_player->PlaceOnGround()) {
				printf("[Warn] Player PlaceOnGround failed at spawn, keeping Y=%.1f\n", sy);
			} else {
				XMVECTOR p = m_player->GetPosition();
				printf("[Info] Player placed on ground at %.2f %.2f %.2f\n",
					XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p));
			}
			if (m_vehicle) {
				XMVECTOR p = m_player->GetPosition();
				m_vehicle->SetPosition(XMVectorGetX(p) + 4.0f, XMVectorGetY(p) + 2.0f, XMVectorGetZ(p));
				m_vehicle->SetHeading(m_player->GetHeading());
				if (!m_vehicle->PlaceOnGround())
					printf("[Warn] Cheetah PlaceOnGround failed\n");
			}
		}
	}

	return true;
}

bool GameWorld::InitWater(DXRender* render)
{
	m_water.reset(new Water());
	if (!m_water->Init(render, GTA_VC_WATERPRO, GTA_VC_PARTICLE_TXD)) {
		printf("[Warn] Water failed to init, continuing without water\n");
		m_water.reset();
		return false;
	}
	printf("[Info] RT water ready — seaY=%.3f\n", m_water->GetSeaLevelY());
	return true;
}

void GameWorld::Shutdown()
{
	if (m_player) {
		m_player->Cleanup();
		m_player.reset();
	}
	if (m_vehicle) {
		m_vehicle->Cleanup();
		m_vehicle.reset();
	}
	if (m_ifp) {
		m_ifp->Cleanup();
		m_ifp.reset();
	}
	if (m_collision) {
		m_collision->SetDebugDrawer(nullptr);
		m_collision->Clear();
		m_collision.reset();
	}
	if (m_clouds) {
		m_clouds->Cleanup();
		m_clouds.reset();
	}
	if (m_water) {
		m_water->Cleanup();
		m_water.reset();
	}
}
