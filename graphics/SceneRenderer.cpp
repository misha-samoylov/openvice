#include "graphics/SceneRenderer.h"
#include "core/GameConfig.h"
#include "core/AlphaFilter.h"

#include <stdio.h>
#include <cmath>

bool SceneRenderer::Init(DXRender* render)
{
	m_physicsDebug.reset(new PhysicsDebugDraw());
	if (!m_physicsDebug->Init(render)) {
		printf("[Warn] PhysicsDebugDraw init failed — F3 debug lines disabled\n");
		m_physicsDebug->Cleanup();
		m_physicsDebug.reset();
	}

	m_shadowMap.reset(new ShadowMap());
	if (FAILED(m_shadowMap->Init(render))) {
		printf("[Error] ShadowMap init failed — continuing without dynamic shadows\n");
		m_shadowMap->Cleanup();
		m_shadowMap.reset();
	}

	m_ssao.reset(new SSAO());
	if (FAILED(m_ssao->Init(render))) {
		printf("[Error] SSAO init failed — continuing without ambient occlusion\n");
		m_ssao->Cleanup();
		m_ssao.reset();
	}

	m_postFX.reset(new PostFX());
	if (FAILED(m_postFX->Init(render))) {
		printf("[Error] PostFX init failed — continuing without colour filter\n");
		m_postFX->Cleanup();
		m_postFX.reset();
	}

	m_godRays.reset(new GodRays());
	if (FAILED(m_godRays->Init(render))) {
		printf("[Error] GodRays init failed — continuing without volumetric rays\n");
		m_godRays->Cleanup();
		m_godRays.reset();
	}

	return true;
}

void SceneRenderer::Shutdown()
{
	if (m_shadowMap) {
		m_shadowMap->Cleanup();
		m_shadowMap.reset();
	}
	if (m_ssao) {
		m_ssao->Cleanup();
		m_ssao.reset();
	}
	if (m_postFX) {
		m_postFX->Cleanup();
		m_postFX.reset();
	}
	if (m_godRays) {
		m_godRays->Cleanup();
		m_godRays.reset();
	}
	if (m_physicsDebug) {
		m_physicsDebug->Cleanup();
		m_physicsDebug.reset();
	}
}

static void FillCascadeMatrices(MeshRenderContext& ctx, ShadowMap* shadows)
{
	if (!shadows) {
		for (UINT i = 0; i < ShadowMap::NUM_CASCADES; i++)
			ctx.lightViewProj[i] = XMMatrixIdentity();
		ctx.cascadeSplits = XMFLOAT4(25.0f, 80.0f, 200.0f, 500.0f);
		return;
	}
	for (UINT i = 0; i < ShadowMap::NUM_CASCADES; i++)
		ctx.lightViewProj[i] = shadows->GetLightViewProj(i);
	ctx.cascadeSplits = shadows->GetSplitDistances();
}

void SceneRenderer::Render(DXRender* render, Camera* camera, Scene& scene, GameWorld& world)
{
	XMMATRIX view = camera->GetView();
	XMMATRIX proj = camera->GetProjection();
	m_frustum.ConstructFrustum(CAMERA_FAR_PLANE, proj, view);

	XMVECTOR focus = camera->GetPosition();
	float focusX = XMVectorGetX(focus);
	float focusY = XMVectorGetY(focus);
	float focusZ = XMVectorGetZ(focus);

	MeshRenderContext ctx;
	ctx.fogColor = XMFLOAT4(SKY_COLOR_R, SKY_COLOR_G, SKY_COLOR_B, SKY_COLOR_A);
	ctx.fogStart = CAMERA_FAR_PLANE * FOG_START_FACTOR;
	ctx.fogEnd = CAMERA_FAR_PLANE * FOG_END_FACTOR;
	ctx.shadowBias = 0.00015f;
	ctx.receiveShadows = 1.0f;
	ctx.windTime = world.WindTime();

	RenderSettings& settings = world.Settings();
	const bool shadowsOn = m_shadowMap && settings.shadowsEnabled;

	/* Open command list + clear main targets before any draws (incl. shadows). */
	render->RenderStart();

	if (shadowsOn) {
		m_shadowMap->UpdateCascades(focusX, focusY, focusZ);
		FillCascadeMatrices(ctx, m_shadowMap.get());

		ctx.pass = MESH_PASS_SHADOW;
		ctx.receiveShadows = 0.0f;

		for (UINT c = 0; c < ShadowMap::NUM_CASCADES; c++) {
			m_shadowMap->Begin(render, c);
			ctx.ClearBindings();
			ctx.viewProj = m_shadowMap->GetLightViewProj(c);
			const float cullRange = m_shadowMap->GetCascadeHalfExtent(c);

			scene.Draw(render, ctx, m_frustum, scene.Opaque(), AlphaFilter::All,
				focusX, focusY, focusZ, cullRange);
			scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::OpaqueOnly,
				focusX, focusY, focusZ, cullRange);
			scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Cutout,
				focusX, focusY, focusZ, cullRange);

			if (world.GetVehicle())
				world.GetVehicle()->Render(render, ctx);
			if (world.GetPlayer() && !world.ControllingVehicle())
				world.GetPlayer()->Render(render, ctx);

			m_shadowMap->End(render);
		}
		ctx.ClearBindings();
	}

	ctx.pass = MESH_PASS_COLOR;
	ctx.viewProj = XMMatrixMultiply(view, proj);
	FillCascadeMatrices(ctx, shadowsOn ? m_shadowMap.get() : nullptr);
	ctx.receiveShadows = shadowsOn ? 1.0f : 0.0f;
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;

	XMVECTOR sunDir;
	if (m_shadowMap) {
		sunDir = m_shadowMap->GetSunDirection();
	} else {
		const float zenith = XMConvertToRadians(ShadowMap::SUN_ZENITH_OFFSET_DEG);
		sunDir = XMVector3Normalize(XMVectorSet(0.0f, cosf(zenith), sinf(zenith), 0.0f));
	}

	if (world.GetClouds())
		world.GetClouds()->Render(render, camera, sunDir, settings.cloudsEnabled);
	ctx.ClearBindings();
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;
	ctx.receiveShadows = shadowsOn ? 1.0f : 0.0f;

	render->SetOpaqueState();
	render->ApplyRasterizerState();
	scene.Draw(render, ctx, m_frustum, scene.Opaque(), AlphaFilter::All);
	scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::OpaqueOnly);

	if (world.GetVehicle())
		world.GetVehicle()->Render(render, ctx);
	if (world.GetPlayer() && !world.ControllingVehicle())
		world.GetPlayer()->Render(render, ctx);

	if (world.GetWater()) {
		const bool reflectClouds = settings.cloudsEnabled && world.GetClouds() != nullptr;
		world.GetWater()->Render(render, camera, m_frustum, CAMERA_FAR_PLANE, sunDir,
			reflectClouds);
	}

	ctx.ClearBindings();
	ctx.viewProj = XMMatrixMultiply(view, proj);
	FillCascadeMatrices(ctx, shadowsOn ? m_shadowMap.get() : nullptr);
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;
	ctx.receiveShadows = shadowsOn ? 1.0f : 0.0f;

	scene.SortAlphaBackToFront(camera);

	render->SetCutoutAlphaState();
	render->ApplyRasterizerState();
	scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Cutout);

	render->SetSoftAlphaState();
	render->ApplyRasterizerState();
	scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Soft);

	if (settings.physicsDebugVisible && m_physicsDebug && world.Collision()) {
		XMVECTOR camPos = camera->GetPosition();
		m_physicsDebug->BeginFrame();
		m_physicsDebug->SetViewProjection(ctx.viewProj);
		m_physicsDebug->SetCullSphere(
			XMVectorGetX(camPos), XMVectorGetY(camPos), XMVectorGetZ(camPos), 120.0f);
		world.Collision()->DebugDrawWorld(settings.physicsDebugFilter);
		render->SetOpaqueState();
		m_physicsDebug->Render(render);
	}

	const bool needDepth =
		(m_ssao && settings.ssaoEnabled) ||
		(m_godRays && settings.godRaysEnabled);
	if (needDepth)
		render->ResolveDepthForSSAO();

	if (m_ssao && settings.ssaoEnabled)
		m_ssao->Apply(render, camera);

	render->ResolveMSAA();

	if (m_godRays && settings.godRaysEnabled) {
		render->ResolveDepthForSSAO();
		m_godRays->Apply(render, camera, sunDir);
	}

	if (m_postFX)
		m_postFX->Apply(render);

	render->RenderEnd();
}
