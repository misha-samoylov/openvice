#pragma once

#include <DirectXMath.h>
#include <vector>

#include "DXRender.hpp"
#include "Mesh.hpp"
#include "Model.h"
#include "loaders/IMG.hpp"
#include "loaders/COL.hpp"
#include "collision/ColTypes.h"

class CollisionWorld;
class btRigidBody;
class btCompoundShape;
class btCollisionShape;
class btRaycastVehicle;
class btVehicleRaycaster;
class btMotionState;

using namespace DirectX;

/* Vice City Cheetah is model 145 (not SA 429). */
static const int MI_CHEETAH = 145;
/* default.ide: cheetah uses wheel_sport (250), scale 0.7 */
static const int MI_WHEEL_SPORT = 250;

enum VehicleWheel {
	WHEEL_FL = 0,
	WHEEL_FR = 1,
	WHEEL_RL = 2,
	WHEEL_RR = 3,
	WHEEL_COUNT = 4
};

class Vehicle
{
public:
	bool Init(Model* model, ColModel* col, CollisionWorld* world, IMG* img, DXRender* render);
	void Cleanup();

	void SetCollisionWorld(CollisionWorld* world);

	/* throttle: -1..1 (S/W), steer: -1..1 (A/D), handbrake: Space.
	 * Applies Bullet vehicle input; call CollisionWorld::Step after Update. */
	void Update(float dt, float throttle, float steer, bool handbrake);
	/* Call after CollisionWorld::Step to refresh pose for camera/render. */
	void SyncPhysics();
	void Render(DXRender* render, MeshRenderContext& ctx);

	XMVECTOR GetPosition() const;
	float GetHeading() const { return m_heading; }
	void SetPosition(float x, float y, float z);
	void SetHeading(float heading);
	bool PlaceOnGround();

	bool IsReady() const { return m_model != nullptr; }
	void SetWireframe(bool enabled) { m_wireframe = enabled; }
	bool IsWireframe() const { return m_wireframe; }

private:
	void ProcessControlInputs(float throttle, float steer, bool handbrake);
	void CreateBulletVehicle();
	void DestroyBulletVehicle();
	void SyncFromBullet();
	void WarpChassis(float x, float y, float z);
	bool LoadWheelMeshes(IMG* img, DXRender* render);
	bool LoadWheelDummies(IMG* img);

	Model* m_model;
	ColModel* m_col;
	CollisionWorld* m_world;

	btRigidBody* m_chassisBody;
	btCompoundShape* m_chassisShape;
	btMotionState* m_motionState;
	btVehicleRaycaster* m_vehicleRaycaster;
	btRaycastVehicle* m_rayVehicle;
	std::vector<btCollisionShape*> m_childShapes;

	float m_posX, m_posY, m_posZ;
	float m_velX, m_velY, m_velZ;
	float m_heading;
	float m_yawRate;

	float m_gasPedal;
	float m_brakePedal;
	float m_steerAngle; /* radians */
	float m_steerInput;

	/* CHEETAH handling.cfg (converted toward SI / openvice dt). */
	float m_mass;
	float m_engineAccel;
	float m_brakeDecel;
	float m_steerLock;
	float m_suspForce;
	float m_suspDamp;
	float m_suspUpper;
	float m_suspLower;
	float m_wheelRadius;
	float m_wheelScale;
	float m_traction;
	float m_maxSpeed;
	float m_heightAboveRoad;

	/* Wheel local positions (engine space: X right, Y up, Z forward). */
	ColVec3 m_wheelLocal[WHEEL_COUNT];
	float m_wheelRestY[WHEEL_COUNT]; /* suspension rest Y in engine local */
	float m_wheelRotation[WHEEL_COUNT]; /* axle spin radians */
	float m_springRatio[WHEEL_COUNT];
	float m_springLength;
	float m_lineLength;
	int m_wheelsOnGround;
	bool m_wheelIsLeft[WHEEL_COUNT];
	bool m_wheelIsFront[WHEEL_COUNT];

	/* Shared wheel_sport meshes (cloned visually at each dummy). */
	std::vector<Mesh*> m_wheelMeshes;

	bool m_wireframe;
};
