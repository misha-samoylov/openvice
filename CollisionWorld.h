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

/* How a ColModel became a Bullet shape — used by F3 debug filter. */
enum ColShapeKind {
	COL_SHAPE_MESH = 0,       /* BVH triangle mesh */
	COL_SHAPE_COMPOUND = 1,   /* COL spheres / boxes */
	COL_SHAPE_BOUNDBOX = 2    /* empty COL (no body; bound only in file header) */
};

/* F3 physics overlay filter for static world bodies. */
enum ColDebugFilter {
	COL_DEBUG_ALL = 0,
	COL_DEBUG_COMPOUND = 1,     /* compound prims + boundBox fallback */
	COL_DEBUG_BOUNDBOX_ONLY = 2 /* only boundBox fallback bodies */
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
	/* filter: COL_DEBUG_* — hides mesh (or non-boundBox) statics via visualize flag. */
	void DebugDrawWorld(int filter = COL_DEBUG_ALL);

	/* re3 CWorld::FindGroundZFor3DCoord analogue — engine Y is up. */
	bool FindGroundY(float x, float y, float z, float* outY) const;

	/*
	 * Vertical foot probe like CPed::ProcessEntityCollision foot line.
	 * Returns true if ground found; outY is ped origin Y (= ground + FEET_OFFSET).
	 */
	bool ProbeFeet(float x, float y, float z, bool wasStanding, float* outPedY) const;

	/* Vertical cast for suspension / ground (engine Y-up). */
	bool CastDownLine(float x, float z, float startY, float endY, float* hitY, ColVec3* hitNormal = nullptr) const;

	/* Closest hit along a segment (engine Y-up). Used for occlusion culling. */
	bool RayTestClosest(
		float x0, float y0, float z0,
		float x1, float y1, float z1,
		float* hitX, float* hitY, float* hitZ,
		ColVec3* hitNormal = nullptr) const;

	size_t GetInstanceCount() const { return m_staticBodies.size(); }

private:
	struct ShapeEntry {
		ColModel* model;
		btCollisionShape* shape;
		btTriangleMesh* triangleMesh; /* owns vertex/index copy for BVH mesh */
		std::vector<btCollisionShape*> ownedChildren;
		int kind; /* ColShapeKind */
	};

	int FindShapeKind(ColModel* model) const;

	btCollisionShape* GetOrCreateShape(ColModel* model);
	btCollisionShape* CreateMeshShape(ColModel* model, ShapeEntry& entry);
	btCollisionShape* CreateCompoundShape(ColModel* model, ShapeEntry& entry);
	void DestroyShapeEntry(ShapeEntry& entry);

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
