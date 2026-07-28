#include "app/App.h"
#include "core/GameConfig.h"
#include "Utils.hpp"

#include <stdio.h>

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

	ContentLoader loader;
	loader.LoadMapContent(m_img.get(), m_render.get(), m_assets);

	m_scene.BuildFromAssets(m_assets, WORLD_HOUR);

	m_world.InitFromAssets(
		m_assets, m_scene, m_img.get(), m_render.get(), m_renderer.PhysicsDebug());

	m_session.InitCameraState();

	printf("[Info] %s loaded\n", PROJECT_NAME);
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
