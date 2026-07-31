#include "gameplay/GameSession.h"
#include "core/GameConfig.h"

#include <stdio.h>
#include <cmath>

GameSession::GameSession()
	: m_camYaw(0.0f)
	, m_camPitch(0.25f)
	, m_camDistance(14.0f)
	, m_freeCamSpeed(1.0f)
	, m_freeCamera(false)
	, m_numpad1WasDown(false)
	, m_numpad0WasDown(false)
	, m_numpad2WasDown(false)
{
	ZeroMemory(&m_mouseLastState, sizeof(m_mouseLastState));
	ZeroMemory(&m_mouseCurrState, sizeof(m_mouseCurrState));
}

void GameSession::InitCameraState()
{
	m_camYaw = 0.0f;
	m_camPitch = 0.25f;
	m_camDistance = 14.0f;
	m_freeCamSpeed = 1.0f;
	m_freeCamera = false;
}

void GameSession::SetFreeCamLook(float yaw, float pitch)
{
	m_camYaw = yaw;
	m_camPitch = pitch;
	m_freeCamera = true;
}

void GameSession::HandleDebugHotkeys(
	Input* input, DXRender* render, GameWorld& world, SceneRenderer& renderer)
{
	static bool f1WasDown = false;
	bool f1Down = input->IsKey(DIK_F1);
	if (f1Down && !f1WasDown) {
		bool on = !render->IsWireframe();
		if (on)
			render->ChangeRasterizerStateToWireframe();
		else
			render->ChangeRasterizerStateToSolid();
		if (world.GetVehicle())
			world.GetVehicle()->SetWireframe(on);
		printf("[Info] World+Cheetah wireframe %s (F1)\n", on ? "ON" : "OFF");
	}
	f1WasDown = f1Down;

	static bool f2WasDown = false;
	bool f2Down = input->IsKey(DIK_F2);
	if (f2Down && !f2WasDown) {
		render->ChangeRasterizerStateToSolid();
		if (world.GetVehicle())
			world.GetVehicle()->SetWireframe(false);
		printf("[Info] Wireframe OFF (F2)\n");
	}
	f2WasDown = f2Down;

	static bool f3WasDown = false;
	bool f3Down = input->IsKey(DIK_F3);
	if (f3Down && !f3WasDown && renderer.PhysicsDebug()) {
		int next = renderer.PhysicsDebug()->GetOverlayMode() + 1;
		if (next > 3)
			next = 0;
		renderer.PhysicsDebug()->SetOverlayMode(next);
		world.Settings().physicsDebugVisible = (next != 0);
		if (next == 1)
			world.Settings().physicsDebugFilter = COL_DEBUG_ALL;
		else if (next == 2)
			world.Settings().physicsDebugFilter = COL_DEBUG_COMPOUND;
		else if (next == 3)
			world.Settings().physicsDebugFilter = COL_DEBUG_BOUNDBOX_ONLY;

		if (world.Collision()) {
			world.Collision()->SetDebugDrawer(
				world.Settings().physicsDebugVisible ? renderer.PhysicsDebug() : nullptr);
		}

		const char* label = "OFF";
		if (next == 1) label = "ALL wireframes";
		else if (next == 2) label = "compound prims only (cyan)";
		else if (next == 3) label = "empty COL (none — skipped)";
		printf("[Info] Physics debug: %s (F3)\n", label);
	}
	f3WasDown = f3Down;

	static bool f7WasDown = false;
	bool f7Down = input->IsKey(DIK_F7);
	if (f7Down && !f7WasDown && (renderer.GetRtBounce() || renderer.Shadows())) {
		world.Settings().shadowsEnabled = !world.Settings().shadowsEnabled;
		printf("[Info] Shadows %s (F7)\n", world.Settings().shadowsEnabled ? "ON" : "OFF");
	}
	f7WasDown = f7Down;

	static bool f8WasDown = false;
	bool f8Down = input->IsKey(DIK_F8);
	if (f8Down && !f8WasDown && renderer.GetRtBounce()) {
		world.Settings().rtaoEnabled = !world.Settings().rtaoEnabled;
		printf("[Info] RTAO %s (F8)\n", world.Settings().rtaoEnabled ? "ON" : "OFF");
	}
	f8WasDown = f8Down;

	static bool f9WasDown = false;
	bool f9Down = input->IsKey(DIK_F9);
	if (f9Down && !f9WasDown && renderer.GetPostFX()) {
		PostFX::Mode mode = renderer.GetPostFX()->CycleMode();
		printf("[Info] PostFX %s (F9)\n", PostFX::ModeName(mode));
	}
	f9WasDown = f9Down;

	static bool f10WasDown = false;
	bool f10Down = input->IsKey(DIK_F10);
	if (f10Down && !f10WasDown && renderer.GetGodRays()) {
		world.Settings().godRaysEnabled = !world.Settings().godRaysEnabled;
		printf("[Info] God rays %s (F10)\n", world.Settings().godRaysEnabled ? "ON" : "OFF");
	}
	f10WasDown = f10Down;

	static bool np9WasDown = false;
	bool np9Down = input->IsKey(DIK_NUMPAD9);
	if (np9Down && !np9WasDown && world.GetClouds()) {
		world.Settings().cloudsEnabled = !world.Settings().cloudsEnabled;
		printf("[Info] Clouds %s (NUMPAD9)\n", world.Settings().cloudsEnabled ? "ON" : "OFF");
	}
	np9WasDown = np9Down;
}

void GameSession::HandleCameraModeHotkeys(Input* input, Camera* camera, GameWorld& world)
{
	bool np1 = input->IsKey(DIK_NUMPAD1);
	bool np0 = input->IsKey(DIK_NUMPAD0);
	bool np2 = input->IsKey(DIK_NUMPAD2);

	if (np1 && !m_numpad1WasDown && !m_freeCamera) {
		m_freeCamera = true;
		XMVECTOR cp = camera->GetPosition();
		camera->SetPosition(XMVectorGetX(cp), XMVectorGetY(cp), XMVectorGetZ(cp));
		printf("[Info] Free camera (NUMPAD1)\n");
	}
	if (np0 && !m_numpad0WasDown) {
		if (m_freeCamera) {
			m_freeCamera = false;
			XMVECTOR cp = camera->GetPosition();
			float cx = XMVectorGetX(cp);
			float cy = XMVectorGetY(cp);
			float cz = XMVectorGetZ(cp);
			/* NUMPAD0 = Tommy: leave free-cam into character control at this spot. */
			if (world.ControllingVehicle()) {
				world.SetControllingVehicle(false);
				if (world.GetPlayer())
					world.GetPlayer()->SetCollisionEnabled(true);
			}
			if (world.GetPlayer()) {
				world.GetPlayer()->SetPosition(cx, cy + 1.0f, cz);
				world.GetPlayer()->PlaceOnGround();
			}
			printf("[Info] Follow camera (NUMPAD0) - Tommy at free-cam position\n");
		} else if (world.ControllingVehicle() && world.GetPlayer() && world.GetVehicle()) {
			world.SetControllingVehicle(false);
			XMVECTOR p = world.GetVehicle()->GetPosition();
			world.GetPlayer()->SetPosition(
				XMVectorGetX(p) + 2.0f, XMVectorGetY(p) + 1.0f, XMVectorGetZ(p));
			world.GetPlayer()->PlaceOnGround();
			world.GetPlayer()->SetCollisionEnabled(true);
			printf("[Info] Controlling Tommy (NUMPAD0)\n");
		}
	}
	if (np2 && !m_numpad2WasDown && world.GetVehicle()) {
		if (m_freeCamera) {
			m_freeCamera = false;
			XMVECTOR cp = camera->GetPosition();
			if (world.GetPlayer())
				world.GetPlayer()->SetCollisionEnabled(false);
			world.SetControllingVehicle(true);
			world.GetVehicle()->SetPosition(
				XMVectorGetX(cp), XMVectorGetY(cp) + 2.5f, XMVectorGetZ(cp));
			world.GetVehicle()->PlaceOnGround();
			printf("[Info] Driving Cheetah (NUMPAD2) - from free-cam\n");
		} else {
			world.SetControllingVehicle(!world.ControllingVehicle());
			if (world.ControllingVehicle()) {
				if (world.GetPlayer())
					world.GetPlayer()->SetCollisionEnabled(false);
				if (world.GetPlayer()) {
					XMVECTOR p = world.GetPlayer()->GetPosition();
					world.GetVehicle()->SetPosition(
						XMVectorGetX(p), XMVectorGetY(p) + 2.5f, XMVectorGetZ(p));
					world.GetVehicle()->SetHeading(world.GetPlayer()->GetHeading());
					world.GetVehicle()->PlaceOnGround();
				}
				printf("[Info] Driving Cheetah (NUMPAD2) - Tommy collision OFF\n");
			} else {
				if (world.GetPlayer() && world.GetVehicle()) {
					XMVECTOR p = world.GetVehicle()->GetPosition();
					world.GetPlayer()->SetPosition(
						XMVectorGetX(p) + 2.0f, XMVectorGetY(p) + 1.0f, XMVectorGetZ(p));
					world.GetPlayer()->PlaceOnGround();
					world.GetPlayer()->SetCollisionEnabled(true);
				}
				printf("[Info] Controlling Tommy (NUMPAD2) - collision ON\n");
			}
		}
	}
	m_numpad1WasDown = np1;
	m_numpad0WasDown = np0;
	m_numpad2WasDown = np2;
}

void GameSession::UpdateMovement(float frameTime, Input* input, Camera* camera, GameWorld& world)
{
	const float camDistanceMin = 3.0f;
	const float camDistanceMax = 40.0f;
	const float freeCamSpeedMin = 0.1f;
	const float freeCamSpeedMax = 50.0f;

	float speed = 10.0f * m_freeCamSpeed * frameTime;
	if (m_freeCamera && (input->IsKey(DIK_LSHIFT) || input->IsKey(DIK_RSHIFT)))
		speed *= 5.0f;

	float moveLeftRight = 0.0f;
	float moveBackForward = 0.0f;
	if (input->IsKey(DIK_W)) moveBackForward += 1.0f;
	if (input->IsKey(DIK_A)) moveLeftRight -= 1.0f;
	if (input->IsKey(DIK_S)) moveBackForward -= 1.0f;
	if (input->IsKey(DIK_D)) moveLeftRight += 1.0f;

	m_mouseCurrState.lX = input->GetMouseSpeedX();
	m_mouseCurrState.lY = input->GetMouseSpeedY();

	if ((m_mouseCurrState.lX != m_mouseLastState.lX)
		|| (m_mouseCurrState.lY != m_mouseLastState.lY)) {
		if (m_freeCamera) {
			m_camYaw += m_mouseCurrState.lX * 0.001f;
			m_camPitch += m_mouseCurrState.lY * 0.001f;
			if (m_camPitch > 1.55f) m_camPitch = 1.55f;
			if (m_camPitch < -1.55f) m_camPitch = -1.55f;
		} else {
			m_camYaw -= m_mouseCurrState.lX * 0.001f;
			m_camPitch -= m_mouseCurrState.lY * 0.001f;
		}
		m_mouseLastState = m_mouseCurrState;
	}

	float wheel = input->GetMouseWheel();
	if (wheel != 0.0f) {
		if (m_freeCamera) {
			float factor = 1.0f + wheel * 0.0015f;
			if (factor < 0.5f) factor = 0.5f;
			if (factor > 2.0f) factor = 2.0f;
			m_freeCamSpeed *= factor;
			if (m_freeCamSpeed < freeCamSpeedMin) m_freeCamSpeed = freeCamSpeedMin;
			if (m_freeCamSpeed > freeCamSpeedMax) m_freeCamSpeed = freeCamSpeedMax;
		} else {
			m_camDistance -= wheel * 0.02f;
			if (m_camDistance < camDistanceMin) m_camDistance = camDistanceMin;
			if (m_camDistance > camDistanceMax) m_camDistance = camDistanceMax;
		}
	}

	if (!m_freeCamera && world.ControllingVehicle() && world.GetVehicle()) {
		float throttle = moveBackForward;
		float steer = moveLeftRight;
		bool handbrake = input->IsKey(DIK_SPACE);
		world.GetVehicle()->Update(frameTime, throttle, steer, handbrake);
		if (world.GetPlayer())
			world.GetPlayer()->Update(frameTime, 0.0f, 0.0f, false, false, false, false);

		if (world.Collision())
			world.Collision()->Step(frameTime);
		world.GetVehicle()->SyncPhysics();
		if (world.GetPlayer())
			world.GetPlayer()->SyncPhysics();

		XMVECTOR p = world.GetVehicle()->GetPosition();
		camera->Follow(
			XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p),
			m_camYaw, m_camPitch, m_camDistance, 0.9f
		);
	} else if (world.GetPlayer() && !m_freeCamera) {
		float s = sinf(m_camYaw);
		float c = cosf(m_camYaw);
		float mx = -moveBackForward * s + moveLeftRight * c;
		float mz = moveBackForward * c + moveLeftRight * s;
		bool moving = (moveLeftRight != 0.0f || moveBackForward != 0.0f);
		bool walking = moving && (input->IsKey(DIK_LMENU) || input->IsKey(DIK_RMENU));
		bool sprinting = moving && (input->IsKey(DIK_LSHIFT) || input->IsKey(DIK_RSHIFT));
		static bool spaceWasDown = false;
		bool spaceDown = input->IsKey(DIK_SPACE);
		bool jump = spaceDown && !spaceWasDown;
		spaceWasDown = spaceDown;

		world.GetPlayer()->Update(frameTime, mx, mz, moving, walking, sprinting, jump);
		if (world.GetVehicle())
			world.GetVehicle()->Update(frameTime, 0.0f, 0.0f, false);

		if (world.Collision())
			world.Collision()->Step(frameTime);
		world.GetPlayer()->SyncPhysics();
		if (world.GetVehicle())
			world.GetVehicle()->SyncPhysics();

		XMVECTOR p = world.GetPlayer()->GetPosition();
		camera->Follow(
			XMVectorGetX(p), XMVectorGetY(p), XMVectorGetZ(p),
			m_camYaw, m_camPitch, m_camDistance, 0.95f
		);
	} else {
		if (world.GetVehicle())
			world.GetVehicle()->Update(frameTime, 0.0f, 0.0f, false);
		if (world.GetPlayer())
			world.GetPlayer()->Update(frameTime, 0.0f, 0.0f, false, false, false, false);
		if (world.Collision())
			world.Collision()->Step(frameTime);
		if (world.GetVehicle())
			world.GetVehicle()->SyncPhysics();
		if (world.GetPlayer())
			world.GetPlayer()->SyncPhysics();
		camera->Update(m_camPitch, m_camYaw, moveLeftRight * speed, moveBackForward * speed);
	}

	if (world.GetWater())
		world.GetWater()->Update(frameTime);
	if (world.GetClouds())
		world.GetClouds()->Update(frameTime, camera);
	world.AdvanceWind(frameTime);
}

void GameSession::HandleFrame(
	float frameTime,
	Input* input,
	Camera* camera,
	DXRender* render,
	GameWorld& world,
	SceneRenderer& renderer)
{
	input->Detect();

	if (input->IsKey(DIK_ESCAPE))
		PostQuitMessage(EXIT_SUCCESS);

	HandleDebugHotkeys(input, render, world, renderer);
	HandleCameraModeHotkeys(input, camera, world);
	UpdateMovement(frameTime, input, camera, world);
}
