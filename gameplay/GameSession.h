#pragma once

#include "Input.hpp"
#include "Camera.hpp"
#include "DXRender.hpp"
#include "world/GameWorld.h"
#include "graphics/SceneRenderer.h"

class GameSession
{
public:
	GameSession();

	void InitCameraState();
	void SetFreeCamLook(float yaw, float pitch);
	void HandleFrame(
		float frameTime,
		Input* input,
		Camera* camera,
		DXRender* render,
		GameWorld& world,
		SceneRenderer& renderer
	);

private:
	void HandleDebugHotkeys(Input* input, DXRender* render, GameWorld& world, SceneRenderer& renderer);
	void HandleCameraModeHotkeys(Input* input, Camera* camera, GameWorld& world);
	void UpdateMovement(float frameTime, Input* input, Camera* camera, GameWorld& world);

	float m_camYaw;
	float m_camPitch;
	float m_camDistance;
	float m_freeCamSpeed;
	bool m_freeCamera;

	bool m_numpad1WasDown;
	bool m_numpad0WasDown;
	bool m_numpad2WasDown;

	DIMOUSESTATE m_mouseLastState;
	DIMOUSESTATE m_mouseCurrState;
};
