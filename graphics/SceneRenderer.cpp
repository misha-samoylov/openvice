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

#if ENABLE_CSM_SHADOWS
	m_shadowMap.reset(new ShadowMap());
	if (FAILED(m_shadowMap->Init(render))) {
		printf("[Error] ShadowMap init failed — continuing without dynamic shadows\n");
		m_shadowMap->Cleanup();
		m_shadowMap.reset();
	}
#else
	(void)render;
	printf("[Info] CSM shadows disabled (ENABLE_CSM_SHADOWS=0)\n");
#endif

#if ENABLE_SSAO
	m_ssao.reset(new SSAO());
	if (FAILED(m_ssao->Init(render))) {
		printf("[Error] SSAO init failed — continuing without ambient occlusion\n");
		m_ssao->Cleanup();
		m_ssao.reset();
	}
#else
	printf("[Info] SSAO disabled (ENABLE_SSAO=0)\n");
#endif

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

#if ENABLE_RT_SHADOWS
	m_rtShadows.reset(new RayTracedShadows());
	if (!m_rtShadows->Init(render)) {
		printf("[Warn] RayTracedShadows init failed — continuing without RT sun shadows\n");
		m_rtShadows->Cleanup();
		m_rtShadows.reset();
	}
#if ENABLE_RT_BOUNCE_PASS
	if (m_rtShadows) {
		m_rtBounce.reset(new RtBouncePass());
		if (FAILED(m_rtBounce->Init(render))) {
			printf("[Warn] RtBouncePass init failed — continuing without RT bounce pass\n");
			m_rtBounce->Cleanup();
			m_rtBounce.reset();
		}
	}
#endif
#if ENABLE_RT_FULL_SCENE
	if (m_rtShadows) {
		m_rtFull.reset(new RtFullScene());
		if (FAILED(m_rtFull->Init(render))) {
			printf("[Warn] RtFullScene init failed — falling back\n");
			FILE* vf = nullptr;
			if (fopen_s(&vf, "openvice_verify.log", "a") == 0 && vf) {
				fputs("[Warn] RtFullScene init failed — falling back\n", vf);
				fclose(vf);
			}
			m_rtFull->Cleanup();
			m_rtFull.reset();
		}
	}
#endif
#endif

	return true;
}

bool SceneRenderer::BuildRayTracing(DXRender* render, const Scene& scene)
{
#if ENABLE_RT_SHADOWS
	if (!m_rtShadows)
		return false;
	if (!m_rtShadows->Build(render, scene)) {
		printf("[Warn] RayTracedShadows Build failed — keeping empty TLAS for binds\n");
		/* Rebuild empty TLAS so root SRV binds stay valid; do not destroy the object. */
		if (!m_rtShadows->Init(render)) {
			m_rtShadows->Cleanup();
			m_rtShadows.reset();
			return false;
		}
		return false;
	}
#if ENABLE_RT_FULL_SCENE
	if (m_rtFull) {
		if (!m_rtFull->BuildShadeData(render, scene, m_rtShadows.get())) {
			printf("[Warn] RtFullScene shade bake failed\n");
			FILE* vf = nullptr;
			if (fopen_s(&vf, "openvice_verify.log", "a") == 0 && vf) {
				fputs("[Warn] RtFullScene shade bake failed\n", vf);
				fclose(vf);
			}
			m_rtFull->Cleanup();
			m_rtFull.reset();
		} else {
			render->FlushUploads();
		}
	}
#endif
	return true;
#else
	(void)render;
	(void)scene;
	return false;
#endif
}

void SceneRenderer::Shutdown()
{
#if ENABLE_RT_SHADOWS
	if (m_rtFull) {
		m_rtFull->Cleanup();
		m_rtFull.reset();
	}
	if (m_rtBounce) {
		m_rtBounce->Cleanup();
		m_rtBounce.reset();
	}
	if (m_rtShadows) {
		m_rtShadows->Cleanup();
		m_rtShadows.reset();
	}
#endif
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

static XMVECTOR ComputeSunDirection(ShadowMap* shadowMap)
{
	if (shadowMap)
		return shadowMap->GetSunDirection();
	const float zenith = XMConvertToRadians(ShadowMap::SUN_ZENITH_OFFSET_DEG);
	return XMVector3Normalize(XMVectorSet(0.0f, cosf(zenith), sinf(zenith), 0.0f));
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
#if ENABLE_CSM_SHADOWS
	const bool shadowsOn = m_shadowMap && settings.shadowsEnabled;
#else
	const bool shadowsOn = false;
	(void)settings.shadowsEnabled;
#endif

#if ENABLE_RT_SHADOWS
	const bool rtOn = m_rtShadows && m_rtShadows->IsReady();
	(void)rtOn;
#else
	const bool rtOn = false;
	(void)rtOn;
#endif

	/* Open command list + clear main targets before any draws (incl. shadows). */
	render->RenderStart();

#if ENABLE_CSM_SHADOWS
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
				focusX, focusY, focusZ, DRAW_DISTANCE, nullptr,
				focusX, focusY, focusZ, cullRange);
			scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::OpaqueOnly,
				focusX, focusY, focusZ, DRAW_DISTANCE, nullptr,
				focusX, focusY, focusZ, cullRange);
			scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Cutout,
				focusX, focusY, focusZ, DRAW_DISTANCE, nullptr,
				focusX, focusY, focusZ, cullRange);

			if (world.GetVehicle())
				world.GetVehicle()->Render(render, ctx);
			if (world.GetPlayer() && !world.ControllingVehicle())
				world.GetPlayer()->Render(render, ctx);

			m_shadowMap->End(render);
		}
		ctx.ClearBindings();
	}
#else
	(void)focusX;
	(void)focusY;
	(void)focusZ;
#endif

	ctx.pass = MESH_PASS_COLOR;
	ctx.viewProj = XMMatrixMultiply(view, proj);
	FillCascadeMatrices(ctx, shadowsOn ? m_shadowMap.get() : nullptr);
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;

	D3D12_GPU_VIRTUAL_ADDRESS tlasVA = 0;
#if ENABLE_RT_SHADOWS
	if (m_rtShadows)
		tlasVA = m_rtShadows->GetGpuAddress();
	if (tlasVA == 0)
		tlasVA = render->GetFallbackTlasVA();
#endif
	ctx.rtAccelVA = (Mesh::HasRtPixelShader() && tlasVA != 0) ? tlasVA : 0;
	ctx.rtAccelSrvIndex = UINT_MAX;
	ctx.receiveShadows = (Mesh::HasRtPixelShader() && tlasVA != 0 &&
		m_rtShadows && m_rtShadows->IsReady()) ? 1.0f : 0.0f;

	XMVECTOR sunDir = ComputeSunDirection(m_shadowMap.get());
	XMStoreFloat3(&ctx.sunDir, sunDir);

#if ENABLE_RT_FULL_SCENE
	/* Full primary-ray path: skip raster meshes; shade + sun shadow in RtFullScene. */
	if (m_rtFull && m_rtFull->IsReady() && m_rtShadows && m_rtShadows->IsReady()) {
		D3D12_GPU_VIRTUAL_ADDRESS tlas = m_rtShadows->GetGpuAddress();
		if (tlas == 0)
			tlas = render->GetFallbackTlasVA();
		float seaY = 5.5f;
		float waterTime = 0.0f;
		UINT waterTex = UINT_MAX;
		float waterUvU = 0.0f, waterUvV = 0.0f;
		if (world.GetWater() && world.GetWater()->IsReady()) {
			seaY = world.GetWater()->GetSeaLevelY();
			waterTime = world.GetWater()->GetTime();
			waterTex = world.GetWater()->GetTextureSrvIndex();
			waterUvU = world.GetWater()->GetUvU();
			waterUvV = world.GetWater()->GetUvV();
		}
		if (tlas != 0)
			m_rtFull->Apply(render, camera, sunDir, tlas, seaY, waterTime,
				waterTex, waterUvU, waterUvV);

		static int s_frameLog = 0;
		if (s_frameLog < 3) {
			XMVECTOR cam = camera->GetPosition();
			char line[256];
			sprintf_s(line,
				"[Info] Frame %d fullRT=1 seaY=%.2f cam=(%.1f,%.1f,%.1f) tlas=%d\n",
				s_frameLog, seaY,
				XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam),
				tlas != 0 ? 1 : 0);
			printf("%s", line);
			fflush(stdout);
			FILE* vf = nullptr;
			if (fopen_s(&vf, "openvice_verify.log", "a") == 0 && vf) {
				fputs(line, vf);
				fclose(vf);
			}
			s_frameLog++;
		}

		if (m_postFX)
			m_postFX->Apply(render);
		render->RenderEnd();
		return;
	}
#endif

#if !ENABLE_SINGLE_OBJECT_RT_DEMO
	if (world.GetClouds())
		world.GetClouds()->Render(render, camera, sunDir, settings.cloudsEnabled);
#endif
	ctx.ClearBindings();
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;
	ctx.rtAccelVA = (Mesh::HasRtPixelShader() && tlasVA != 0) ? tlasVA : 0;
	ctx.receiveShadows = (Mesh::HasRtPixelShader() && tlasVA != 0 &&
		m_rtShadows && m_rtShadows->IsReady()) ? 1.0f : 0.0f;

#if ENABLE_SINGLE_OBJECT_RT_DEMO
	const CollisionWorld* col = nullptr;
#else
	const CollisionWorld* col = world.Collision();
#endif

	render->SetOpaqueState();
	render->ApplyRasterizerState();
	int drawsOpaque = scene.Draw(render, ctx, m_frustum, scene.Opaque(), AlphaFilter::All,
		focusX, focusY, focusZ, DRAW_DISTANCE, col);
	int drawsAlphaOp = scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::OpaqueOnly,
		focusX, focusY, focusZ, DRAW_DISTANCE, col);

#if !ENABLE_SINGLE_OBJECT_RT_DEMO
	if (world.GetVehicle())
		world.GetVehicle()->Render(render, ctx);
	if (world.GetPlayer() && !world.ControllingVehicle())
		world.GetPlayer()->Render(render, ctx);

	if (world.GetWater()) {
		const bool reflectClouds = settings.cloudsEnabled && world.GetClouds() != nullptr;
		world.GetWater()->Render(render, camera, m_frustum, DRAW_DISTANCE, sunDir,
			reflectClouds);
	}
#endif

	ctx.ClearBindings();
	ctx.viewProj = XMMatrixMultiply(view, proj);
	FillCascadeMatrices(ctx, shadowsOn ? m_shadowMap.get() : nullptr);
	ctx.shadowSrvIndex = shadowsOn ? m_shadowMap->GetSrvIndex() : UINT_MAX;
	ctx.shadowSamplerIndex = shadowsOn ? m_shadowMap->GetCmpSamplerIndex() : UINT_MAX;
	ctx.rtAccelVA = (Mesh::HasRtPixelShader() && tlasVA != 0) ? tlasVA : 0;
	ctx.receiveShadows = (Mesh::HasRtPixelShader() && tlasVA != 0 &&
		m_rtShadows && m_rtShadows->IsReady()) ? 1.0f : 0.0f;

	scene.SortAlphaBackToFront(camera);

	render->SetCutoutAlphaState();
	render->ApplyRasterizerState();
	int drawsCutout = scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Cutout,
		focusX, focusY, focusZ, DRAW_DISTANCE, col);

	render->SetSoftAlphaState();
	render->ApplyRasterizerState();
	int drawsSoft = scene.Draw(render, ctx, m_frustum, scene.Alpha(), AlphaFilter::Soft,
		focusX, focusY, focusZ, DRAW_DISTANCE, col);

	static int s_frameLog = 0;
	if (s_frameLog < 3) {
		XMVECTOR cam = camera->GetPosition();
		char line[512];
		sprintf_s(line,
			"[Info] Frame %d draws opaque=%d alphaOp=%d cutout=%d soft=%d cam=(%.1f,%.1f,%.1f) rtPS=%d rtReady=%d\n",
			s_frameLog,
			drawsOpaque, drawsAlphaOp, drawsCutout, drawsSoft,
			XMVectorGetX(cam), XMVectorGetY(cam), XMVectorGetZ(cam),
			Mesh::HasRtPixelShader() ? 1 : 0,
			(m_rtShadows && m_rtShadows->IsReady()) ? 1 : 0);
		printf("%s", line);
		fflush(stdout);
		FILE* vf = nullptr;
		if (fopen_s(&vf, "openvice_verify.log", "a") == 0 && vf) {
			fputs(line, vf);
			fclose(vf);
		}
		s_frameLog++;
	}

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
#if ENABLE_SSAO
		(m_ssao && settings.ssaoEnabled) ||
#endif
#if ENABLE_RT_BOUNCE_PASS
		(m_rtBounce && m_rtShadows && m_rtShadows->IsReady()) ||
#endif
		(m_godRays && settings.godRaysEnabled);
	if (needDepth)
		render->ResolveDepthForSSAO();

#if ENABLE_SSAO
	if (m_ssao && settings.ssaoEnabled)
		m_ssao->Apply(render, camera);
#endif

#if ENABLE_RT_BOUNCE_PASS
	if (m_rtBounce && m_rtShadows && m_rtShadows->IsReady()) {
		D3D12_GPU_VIRTUAL_ADDRESS tlas = m_rtShadows->GetGpuAddress();
		if (tlas == 0)
			tlas = render->GetFallbackTlasVA();
		if (tlas != 0)
			m_rtBounce->Apply(render, camera, sunDir, tlas);
	}
#endif

	render->ResolveMSAA();

	if (m_godRays && settings.godRaysEnabled) {
		render->ResolveDepthForSSAO();
		m_godRays->Apply(render, camera, sunDir);
	}

	if (m_postFX)
		m_postFX->Apply(render);

	render->RenderEnd();
}
