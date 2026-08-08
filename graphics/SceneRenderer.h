#pragma once

#include <memory>

#include "DXRender.hpp"
#include "Camera.hpp"
#include "Frustum.h"
#include "world/Scene.h"
#include "world/GameWorld.h"
#include "ShadowMap.h"
#include "SSAO.h"
#include "PostFX.h"
#include "GodRays.h"
#include "PhysicsDebugDraw.h"
#include "loaders/IMG.hpp"
#include "ui/Hud.h"

class SceneRenderer
{
public:
	bool Init(DXRender* render);
	bool InitHud(DXRender* render, IMG* img);
	void Shutdown();

	void Render(DXRender* render, Camera* camera, Scene& scene, GameWorld& world, float camYaw, bool showHud = true);

	ShadowMap* Shadows() { return m_shadowMap.get(); }
	SSAO* GetSSAO() { return m_ssao.get(); }
	PostFX* GetPostFX() { return m_postFX.get(); }
	GodRays* GetGodRays() { return m_godRays.get(); }
	PhysicsDebugDraw* PhysicsDebug() { return m_physicsDebug.get(); }
	Hud* GetHud() { return m_hud.get(); }

private:
	std::unique_ptr<ShadowMap> m_shadowMap;
	std::unique_ptr<SSAO> m_ssao;
	std::unique_ptr<PostFX> m_postFX;
	std::unique_ptr<GodRays> m_godRays;
	std::unique_ptr<PhysicsDebugDraw> m_physicsDebug;
	std::unique_ptr<Hud> m_hud;
	Frustum m_frustum;
};
