#include "app/App.h"
#include "core/GameConfig.h"
#include "Utils.hpp"
#include "assets/ContentLoader.h"

#include <stdio.h>
#include <vector>

bool App::Initialize(HINSTANCE hInstance, int nCmdShow)
{
	/* .cso land in $(OutDir); VS defaults cwd to $(ProjectDir). */
	Utils::SetWorkingDirectoryToExe();

	if (!DirectX::XMVerifyCPUSupport()) {
		MessageBox(NULL, L"You CPU doesn't support DirectXMath", L"Error", MB_OK);
		return false;
	}

	const bool vsync = false;

	m_window.reset(new Window());
	m_window->Init(hInstance, nCmdShow, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);

	m_input.reset(new Input());
	m_input->Init(hInstance, m_window->GetHandleWindow());

	m_camera.reset(new Camera());
	m_camera->Init(WINDOW_WIDTH, WINDOW_HEIGHT, CAMERA_FAR_PLANE);

	m_render.reset(new DXRender());
	m_render->Init(m_window->GetHandleWindow(), vsync);

	m_renderer.Init(m_render.get());

	m_img.reset(new IMG());
	TCHAR imgPath[] = GTA_VC_IMG_PATH;
	TCHAR dirPath[] = GTA_VC_DIR_PATH;
	m_img->Open(imgPath, dirPath);

#if ENABLE_SINGLE_OBJECT_RT_DEMO
	printf("[Info] RT MAP DEMO — all IPLs (interior 0/13), free camera, water/clouds via RT\n");
	{
		ContentLoader loader;
		loader.LoadMapContent(m_img.get(), m_render.get(), m_assets);
	}
	m_scene.BuildFromAssets(m_assets, WORLD_HOUR);
	if (m_scene.Opaque().empty() && m_scene.Alpha().empty()) {
		printf("[Error] No instances placed from IPL (interior 0/13)\n");
		MessageBoxA(NULL, "Scene build produced an empty map", "Error", MB_OK);
		return false;
	}
	m_world.InitWater(m_render.get());
	/* No player/vehicle/collision — water feeds RT sea plane. */
#else
	ContentLoader loader;
	loader.LoadMapContent(m_img.get(), m_render.get(), m_assets);

	m_scene.BuildFromAssets(m_assets, WORLD_HOUR);

	m_world.InitFromAssets(
		m_assets, m_scene, m_img.get(), m_render.get(), m_renderer.PhysicsDebug());
#endif

	/* Finish pending DEFAULT-buffer / texture uploads from ContentLoader + world. */
	m_render->FlushUploads();
	printf("[Info] SRV descriptors used: %u\n", m_render->GetSrvCount());

#if ENABLE_RT_SHADOWS
	if (!m_renderer.BuildRayTracing(m_render.get(), m_scene))
		printf("[Warn] Scene TLAS not ready — using empty fallback TLAS for RayQuery\n");
#endif

	m_session.InitCameraState();
#if ENABLE_SINGLE_OBJECT_RT_DEMO
	{
		/* Spawn free-cam above the island centroid. */
		double sx = 0, sy = 0, sz = 0;
		size_t n = 0;
		auto accum = [&](const std::vector<SceneInstance>& list) {
			for (size_t i = 0; i < list.size(); i++) {
				sx += list[i].x;
				sy += list[i].y;
				sz += list[i].z;
				n++;
			}
		};
		accum(m_scene.Opaque());
		accum(m_scene.Alpha());
		if (n > 0) {
			float cx = (float)(sx / (double)n);
			float cy = (float)(sy / (double)n);
			float cz = (float)(sz / (double)n);
			/* Sit above/south of the island and look toward the centroid. */
			m_camera->SetPosition(cx, cy + 60.0f, cz - 140.0f);
			m_session.SetFreeCamLook(0.0f, 0.28f);
			m_camera->Update(0.28f, 0.0f, 0.0f, 0.0f);
			printf("[Info] Free cam start (%.1f, %.1f, %.1f) look→(%.1f,%.1f,%.1f) instances=%zu\n",
				cx, cy + 60.0f, cz - 140.0f, cx, cy, cz, n);
		}
	}
#endif

	const bool fullRt =
#if ENABLE_RT_FULL_SCENE
		m_renderer.GetRtFull() && m_renderer.GetRtFull()->IsReady();
#else
		false;
#endif

	printf("[Info] %s loaded - opaque=%zu alpha=%zu rtPS=%d fullRT=%d fallbackTlas=0x%llX\n",
		PROJECT_NAME,
		m_scene.Opaque().size(),
		m_scene.Alpha().size(),
		Mesh::HasRtPixelShader() ? 1 : 0,
		fullRt ? 1 : 0,
		(unsigned long long)m_render->GetFallbackTlasVA());
	fflush(stdout);
	{
		FILE* vf = nullptr;
		if (fopen_s(&vf, "openvice_verify.log", "w") == 0 && vf) {
			fprintf(vf, "[Info] %s loaded - opaque=%zu alpha=%zu rtPS=%d fullRT=%d fallbackTlas=0x%llX\n",
				PROJECT_NAME,
				m_scene.Opaque().size(),
				m_scene.Alpha().size(),
				Mesh::HasRtPixelShader() ? 1 : 0,
				fullRt ? 1 : 0,
				(unsigned long long)m_render->GetFallbackTlasVA());
			fclose(vf);
		}
	}
	return true;
}

int App::Run()
{
	int frameCount = 0;
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			frameCount++;
			if (Utils::GetTime() > 1.0f) {
				frameCount = 0;
				Utils::StartTimer();
			}

			float frameTime = (float)Utils::GetFrameTime();
			m_session.HandleFrame(
				frameTime, m_input.get(), m_camera.get(), m_render.get(),
				m_world, m_renderer);
			m_renderer.Render(m_render.get(), m_camera.get(), m_scene, m_world);
		}
	}

	return (int)msg.wParam;
}

void App::Shutdown()
{
	/* GPU may still reference player/mesh/IB resources from in-flight frames. */
	if (m_render)
		m_render->WaitForGpu();

	m_world.Shutdown();
	m_renderer.Shutdown();
	m_assets.Clear();

	if (m_render)
		m_render->Cleanup();
	if (m_camera)
		m_camera->Cleanup();
	if (m_input)
		m_input->Cleanup();
	if (m_img) {
		m_img->Cleanup();
		m_img.reset();
	}

	m_render.reset();
	m_camera.reset();
	m_input.reset();
	m_window.reset();
}
