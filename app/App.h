#pragma once

#include <memory>

#include "Window.hpp"
#include "Input.hpp"
#include "Camera.hpp"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"
#include "assets/AssetRegistry.h"
#include "assets/ContentLoader.h"
#include "world/Scene.h"
#include "world/GameWorld.h"
#include "graphics/SceneRenderer.h"
#include "gameplay/GameSession.h"

class App
{
public:
	bool Initialize(HINSTANCE hInstance, int nCmdShow);
	int Run();
	void Shutdown();

private:
	std::unique_ptr<Window> m_window;
	std::unique_ptr<Input> m_input;
	std::unique_ptr<Camera> m_camera;
	std::unique_ptr<DXRender> m_render;
	std::unique_ptr<IMG> m_img;

	AssetRegistry m_assets;
	Scene m_scene;
	GameWorld m_world;
	SceneRenderer m_renderer;
	GameSession m_session;
};
