#pragma once

#include <vector>
#include <DirectXMath.h>

#include "collision/ColTypes.h"
#include "loaders/COL.hpp"

class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btCollisionShape;
class btRigidBody;
class btTriangleMesh;
class btGhostPairCallback;

using namespace DirectX;

struct ColInstancePlacement {
	ColModel* model;
	float x, y, z;
	float scale[3];
	float rotation[4]; /* xyzw quaternion in engine space */
};

/* World collision + dynamics via Bullet Physics 2.89 (Y-up). */
class CollisionWorld
{
public:
	CollisionWorld();
	~CollisionWorld();

	void Clear();
	void Build(COL* colStore, const std::vector<ColInstancePlacement>& placements);

	/* Advance Bullet simulation (call once per frame after entity inputs). */
	void Step(float dt);

	btDiscreteDynamicsWorld* GetDynamicsWorld() const { return m_dynamicsWorld; }

	void SetDebugDrawer(class btIDebugDraw* drawer);
	void DebugDrawWorld();

	/* re3 CWorld::FindGroundZFor3DCoord analogue — engine Y is up. */
	bool FindGroundY(float x, float y, float z, float* outY) const;

	/*
	 * Vertical foot probe like CPed::ProcessEntityCollision foot line.
	 * Returns true if ground found; outY is ped origin Y (= ground + FEET_OFFSET).
	 */
	bool ProbeFeet(float x, float y, float z, bool wasStanding, float* outPedY) const;

	/* Vertical cast for suspension / ground (engine Y-up). */
	bool CastDownLine(float x, float z, float startY, float endY, float* hitY, ColVec3* hitNormal = nullptr) const;

	/*
	 * Legacy sphere resolve — no-ops under Bullet (character/vehicle own contacts).
	 * Kept so older call sites still compile during migration.
	 */
	void ResolvePedSpheres(float* x, float* y, float* z) const;
	void ResolveSpheres(
		const ColSphere* spheres, int count,
		float* x, float* y, float* z,
		float* vx = nullptr, float* vy = nullptr, float* vz = nullptr) const;

	size_t GetInstanceCount() const { return m_staticBodies.size(); }

private:
	struct ShapeEntry {
		ColModel* model;
		btCollisionShape* shape;
		btTriangleMesh* triangleMesh; /* owns vertex/index copy for BVH mesh */
		std::vector<btCollisionShape*> ownedChildren;
	};

	btCollisionShape* GetOrCreateShape(ColModel* model);
	btCollisionShape* CreateMeshShape(ColModel* model, ShapeEntry& entry);
	btCollisionShape* CreateCompoundShape(ColModel* model, ShapeEntry& entry);
	void DestroyShapeEntry(ShapeEntry& entry);

	bool RayTestClosest(
		float x0, float y0, float z0,
		float x1, float y1, float z1,
		float* hitX, float* hitY, float* hitZ,
		ColVec3* hitNormal) const;

	btDefaultCollisionConfiguration* m_collisionConfig;
	btCollisionDispatcher* m_dispatcher;
	btBroadphaseInterface* m_broadphase;
	btSequentialImpulseConstraintSolver* m_solver;
	btDiscreteDynamicsWorld* m_dynamicsWorld;
	btGhostPairCallback* m_ghostPairCallback;

	std::vector<ShapeEntry> m_shapes;
	std::vector<btCollisionShape*> m_instanceShapes; /* per-placement scaled wrappers */
	std::vector<btRigidBody*> m_staticBodies;
};
