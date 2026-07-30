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
#include "RayTracedShadows.h"
#include "RtBouncePass.h"
#include "RtFullScene.h"
#include "PhysicsDebugDraw.h"

class SceneRenderer
{
public:
	bool Init(DXRender* render);
	void Shutdown();

	/* Call after scene geometry GPU uploads are flushed. */
	bool BuildRayTracing(DXRender* render, const Scene& scene);

	void Render(DXRender* render, Camera* camera, Scene& scene, GameWorld& world);

	ShadowMap* Shadows() { return m_shadowMap.get(); }
	SSAO* GetSSAO() { return m_ssao.get(); }
	PostFX* GetPostFX() { return m_postFX.get(); }
	GodRays* GetGodRays() { return m_godRays.get(); }
	RayTracedShadows* GetRtShadows() { return m_rtShadows.get(); }
	RtFullScene* GetRtFull() { return m_rtFull.get(); }
	PhysicsDebugDraw* PhysicsDebug() { return m_physicsDebug.get(); }

private:
	std::unique_ptr<ShadowMap> m_shadowMap;
	std::unique_ptr<SSAO> m_ssao;
	std::unique_ptr<PostFX> m_postFX;
	std::unique_ptr<GodRays> m_godRays;
	std::unique_ptr<RayTracedShadows> m_rtShadows;
	std::unique_ptr<RtBouncePass> m_rtBounce;
	std::unique_ptr<RtFullScene> m_rtFull;
	std::unique_ptr<PhysicsDebugDraw> m_physicsDebug;
	Frustum m_frustum;
};
