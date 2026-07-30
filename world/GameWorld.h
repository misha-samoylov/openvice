#pragma once

#include <memory>

#include "CollisionWorld.h"
#include "Player.h"
#include "Vehicle.h"
#include "Water.h"
#include "Clouds.h"
#include "loaders/IFP.h"
#include "assets/AssetRegistry.h"
#include "world/Scene.h"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"

class PhysicsDebugDraw;

struct RenderSettings
{
	bool shadowsEnabled;
	bool ssaoEnabled;
	bool godRaysEnabled;
	bool cloudsEnabled;
	bool physicsDebugVisible;
	int physicsDebugFilter;

	RenderSettings()
		: shadowsEnabled(false) /* CSM off; lighting is RT-only */
		, ssaoEnabled(false)
		, godRaysEnabled(false) /* screen-space; lighting is RT sun */
		, cloudsEnabled(true)
		, physicsDebugVisible(false)
		, physicsDebugFilter(COL_DEBUG_ALL)
	{
	}
};

class GameWorld
{
public:
	bool InitFromAssets(
		AssetRegistry& assets,
		Scene& scene,
		IMG* img,
		DXRender* render,
		PhysicsDebugDraw* debugDraw
	);
	/* RT map demo: waterpro + mesh only (no player/vehicle). */
	bool InitWater(DXRender* render);
	void Shutdown();

	CollisionWorld* Collision() { return m_collision.get(); }
	Player* GetPlayer() { return m_player.get(); }
	Vehicle* GetVehicle() { return m_vehicle.get(); }
	Water* GetWater() { return m_water.get(); }
	Clouds* GetClouds() { return m_clouds.get(); }
	IFP* GetIfp() { return m_ifp.get(); }

	bool ControllingVehicle() const { return m_controllingVehicle; }
	void SetControllingVehicle(bool v) { m_controllingVehicle = v; }

	RenderSettings& Settings() { return m_settings; }
	const RenderSettings& Settings() const { return m_settings; }

	float WindTime() const { return m_windTime; }
	void AdvanceWind(float dt)
	{
		if (dt < 0.0f) dt = 0.0f;
		if (dt > 0.1f) dt = 0.1f;
		m_windTime += dt;
	}

private:
	void BuildCollision(AssetRegistry& assets, Scene& scene, PhysicsDebugDraw* debugDraw);
	static void AppendColPlacements(
		COL* col,
		const std::vector<SceneInstance>& instances,
		std::vector<ColInstancePlacement>& out
	);

	std::unique_ptr<CollisionWorld> m_collision;
	std::unique_ptr<Player> m_player;
	std::unique_ptr<Vehicle> m_vehicle;
	std::unique_ptr<Water> m_water;
	std::unique_ptr<Clouds> m_clouds;
	std::unique_ptr<IFP> m_ifp;
	bool m_controllingVehicle = false;
	RenderSettings m_settings;
	float m_windTime = 0.0f;
};
