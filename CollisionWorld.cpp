#include "CollisionWorld.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <float.h>
#include <map>

#define __BT_DISABLE_SSE__
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

using namespace DirectX;

namespace {

const float kFixedTimeStep = 1.0f / 60.0f;

btVector3 ToBt(const ColVec3& v)
{
	return btVector3(v.x, v.y, v.z);
}

} // namespace

CollisionWorld::CollisionWorld()
	: m_collisionConfig(nullptr)
	, m_dispatcher(nullptr)
	, m_broadphase(nullptr)
	, m_solver(nullptr)
	, m_dynamicsWorld(nullptr)
	, m_ghostPairCallback(nullptr)
{
	m_collisionConfig = new btDefaultCollisionConfiguration();
	m_dispatcher = new btCollisionDispatcher(m_collisionConfig);
	m_broadphase = new btDbvtBroadphase();
	m_solver = new btSequentialImpulseConstraintSolver();
	m_dynamicsWorld = new btDiscreteDynamicsWorld(
		m_dispatcher, m_broadphase, m_solver, m_collisionConfig);

	m_dynamicsWorld->setGravity(btVector3(0.0f, -PED_GRAVITY, 0.0f));

	m_ghostPairCallback = new btGhostPairCallback();
	m_broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(m_ghostPairCallback);
}

CollisionWorld::~CollisionWorld()
{
	Clear();

	if (m_dynamicsWorld) {
		delete m_dynamicsWorld;
		m_dynamicsWorld = nullptr;
	}
	if (m_solver) {
		delete m_solver;
		m_solver = nullptr;
	}
	if (m_broadphase) {
		delete m_broadphase;
		m_broadphase = nullptr;
	}
	if (m_dispatcher) {
		delete m_dispatcher;
		m_dispatcher = nullptr;
	}
	if (m_collisionConfig) {
		delete m_collisionConfig;
		m_collisionConfig = nullptr;
	}
	if (m_ghostPairCallback) {
		delete m_ghostPairCallback;
		m_ghostPairCallback = nullptr;
	}
}

void CollisionWorld::DestroyShapeEntry(ShapeEntry& entry)
{
	if (entry.shape) {
		delete entry.shape;
		entry.shape = nullptr;
	}
	if (entry.triangleMesh) {
		delete entry.triangleMesh;
		entry.triangleMesh = nullptr;
	}
	for (size_t i = 0; i < entry.ownedChildren.size(); i++)
		delete entry.ownedChildren[i];
	entry.ownedChildren.clear();
	entry.model = nullptr;
}

void CollisionWorld::Clear()
{
	if (m_dynamicsWorld) {
		for (size_t i = 0; i < m_staticBodies.size(); i++) {
			btRigidBody* body = m_staticBodies[i];
			if (!body)
				continue;
			m_dynamicsWorld->removeRigidBody(body);
			if (body->getUserPointer()) {
				/* Scaled wrappers are owned by m_instanceShapes. */
				body->setUserPointer(nullptr);
			}
			if (body->getMotionState())
				delete body->getMotionState();
			delete body;
		}
	}
	m_staticBodies.clear();

	for (size_t i = 0; i < m_instanceShapes.size(); i++) {
		btCollisionShape* s = m_instanceShapes[i];
		if (!s)
			continue;
		if (s->isCompound()) {
			btCompoundShape* compound = (btCompoundShape*)s;
			for (int ci = 0; ci < compound->getNumChildShapes(); ci++) {
				btCollisionShape* child = compound->getChildShape(ci);
				/* Children newly allocated for scale bake are not in m_shapes. */
				bool shared = false;
				for (size_t si = 0; si < m_shapes.size() && !shared; si++) {
					if (m_shapes[si].shape == child)
						shared = true;
					for (size_t oi = 0; oi < m_shapes[si].ownedChildren.size(); oi++) {
						if (m_shapes[si].ownedChildren[oi] == child) {
							shared = true;
							break;
						}
					}
				}
				if (!shared)
					delete child;
			}
		}
		delete s;
	}
	m_instanceShapes.clear();

	for (size_t i = 0; i < m_shapes.size(); i++)
		DestroyShapeEntry(m_shapes[i]);
	m_shapes.clear();
}

btCollisionShape* CollisionWorld::CreateMeshShape(ColModel* model, ShapeEntry& entry)
{
	const size_t vertCount = model->vertices.size();
	const size_t triCount = model->triangles.size();
	if (vertCount == 0 || triCount == 0)
		return nullptr;

	/*
	 * btTriangleMesh owns a copy of the triangles. Do NOT point Bullet at
	 * std::vector::data() — ShapeEntry is later push_back'd into m_shapes,
	 * which copies/moves vectors and leaves dangling graphicsbase pointers.
	 */
	btTriangleMesh* mesh = new btTriangleMesh(true, false);
	mesh->preallocateVertices((int)vertCount);
	mesh->preallocateIndices((int)(triCount * 3));

	for (size_t i = 0; i < triCount; i++) {
		const ColTriangle& t = model->triangles[i];
		if (t.a >= vertCount || t.b >= vertCount || t.c >= vertCount)
			continue;
		const ColVec3& va = model->vertices[t.a];
		const ColVec3& vb = model->vertices[t.b];
		const ColVec3& vc = model->vertices[t.c];
		mesh->addTriangle(
			btVector3(va.x, va.y, va.z),
			btVector3(vb.x, vb.y, vb.z),
			btVector3(vc.x, vc.y, vc.z),
			true);
	}

	if (mesh->getNumTriangles() <= 0) {
		delete mesh;
		return nullptr;
	}

	entry.triangleMesh = mesh;
	return new btBvhTriangleMeshShape(mesh, true, true);
}

btCollisionShape* CollisionWorld::CreateCompoundShape(ColModel* model, ShapeEntry& entry)
{
	btCompoundShape* compound = new btCompoundShape();

	for (size_t i = 0; i < model->spheres.size(); i++) {
		const ColSphere& s = model->spheres[i];
		btSphereShape* sphere = new btSphereShape(s.radius);
		entry.ownedChildren.push_back(sphere);
		btTransform local;
		local.setIdentity();
		local.setOrigin(ToBt(s.center));
		compound->addChildShape(local, sphere);
	}

	for (size_t i = 0; i < model->boxes.size(); i++) {
		const ColBox& b = model->boxes[i];
		btVector3 half(
			(b.max.x - b.min.x) * 0.5f,
			(b.max.y - b.min.y) * 0.5f,
			(b.max.z - b.min.z) * 0.5f);
		if (half.x() < 0.01f) half.setX(0.01f);
		if (half.y() < 0.01f) half.setY(0.01f);
		if (half.z() < 0.01f) half.setZ(0.01f);
		btBoxShape* box = new btBoxShape(half);
		entry.ownedChildren.push_back(box);
		btTransform local;
		local.setIdentity();
		local.setOrigin(btVector3(
			(b.min.x + b.max.x) * 0.5f,
			(b.min.y + b.max.y) * 0.5f,
			(b.min.z + b.max.z) * 0.5f));
		compound->addChildShape(local, box);
	}

	if (compound->getNumChildShapes() == 0) {
		const ColBox& b = model->boundBox;
		btVector3 half(
			(b.max.x - b.min.x) * 0.5f,
			(b.max.y - b.min.y) * 0.5f,
			(b.max.z - b.min.z) * 0.5f);
		if (half.x() < 0.05f) half.setX(0.05f);
		if (half.y() < 0.05f) half.setY(0.05f);
		if (half.z() < 0.05f) half.setZ(0.05f);
		btBoxShape* box = new btBoxShape(half);
		entry.ownedChildren.push_back(box);
		btTransform local;
		local.setIdentity();
		local.setOrigin(btVector3(
			(b.min.x + b.max.x) * 0.5f,
			(b.min.y + b.max.y) * 0.5f,
			(b.min.z + b.max.z) * 0.5f));
		compound->addChildShape(local, box);
	}

	return compound;
}

btCollisionShape* CollisionWorld::GetOrCreateShape(ColModel* model)
{
	if (!model)
		return nullptr;

	for (size_t i = 0; i < m_shapes.size(); i++) {
		if (m_shapes[i].model == model)
			return m_shapes[i].shape;
	}

	ShapeEntry entry;
	entry.model = model;
	entry.shape = nullptr;
	entry.triangleMesh = nullptr;

	if (!model->triangles.empty() && !model->vertices.empty())
		entry.shape = CreateMeshShape(model, entry);
	else
		entry.shape = CreateCompoundShape(model, entry);

	if (!entry.shape) {
		DestroyShapeEntry(entry);
		return nullptr;
	}

	m_shapes.push_back(std::move(entry));
	return m_shapes.back().shape;
}

void CollisionWorld::Build(COL* colStore, const std::vector<ColInstancePlacement>& placements)
{
	Clear();
	if (!colStore || !m_dynamicsWorld)
		return;

	int matched = 0;
	std::map<ColModel*, btCollisionShape*> shapeCache;

	for (size_t i = 0; i < placements.size(); i++) {
		const ColInstancePlacement& p = placements[i];
		if (!p.model)
			continue;

		btCollisionShape* baseShape = nullptr;
		std::map<ColModel*, btCollisionShape*>::iterator it = shapeCache.find(p.model);
		if (it != shapeCache.end()) {
			baseShape = it->second;
		} else {
			baseShape = GetOrCreateShape(p.model);
			if (baseShape)
				shapeCache[p.model] = baseShape;
		}
		if (!baseShape)
			continue;

		btCollisionShape* useShape = baseShape;
		const bool scaled =
			fabsf(p.scale[0] - 1.0f) > 0.001f ||
			fabsf(p.scale[1] - 1.0f) > 0.001f ||
			fabsf(p.scale[2] - 1.0f) > 0.001f;

		/* Never mutate shared shapes — wrap or clone when scaled. */
		btCollisionShape* scaledWrapper = nullptr;
		if (scaled) {
			btVector3 localScale(p.scale[0], p.scale[1], p.scale[2]);
			if (baseShape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE ||
				baseShape->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE) {
				scaledWrapper = new btScaledBvhTriangleMeshShape(
					(btBvhTriangleMeshShape*)baseShape, localScale);
				useShape = scaledWrapper;
			} else {
				/* Per-instance compound with baked scale via child transforms. */
				btCompoundShape* src = (btCompoundShape*)baseShape;
				btCompoundShape* copy = new btCompoundShape();
				for (int ci = 0; ci < src->getNumChildShapes(); ci++) {
					btTransform child = src->getChildTransform(ci);
					btVector3 o = child.getOrigin();
					o = btVector3(o.x() * localScale.x(), o.y() * localScale.y(), o.z() * localScale.z());
					child.setOrigin(o);
					btCollisionShape* childShape = src->getChildShape(ci);
					int type = childShape->getShapeType();
					btCollisionShape* scaledChild = nullptr;
					if (type == SPHERE_SHAPE_PROXYTYPE) {
						float r = ((btSphereShape*)childShape)->getRadius();
						float s = (localScale.x() + localScale.y() + localScale.z()) / 3.0f;
						scaledChild = new btSphereShape(r * s);
					} else if (type == BOX_SHAPE_PROXYTYPE) {
						btVector3 h = ((btBoxShape*)childShape)->getHalfExtentsWithMargin();
						scaledChild = new btBoxShape(btVector3(
							h.x() * localScale.x(), h.y() * localScale.y(), h.z() * localScale.z()));
					} else {
						scaledChild = childShape;
					}
					if (scaledChild != childShape) {
						/* Track via user pointer list on wrapper body later — store on compound user. */
						copy->addChildShape(child, scaledChild);
					} else {
						copy->addChildShape(child, childShape);
					}
				}
				scaledWrapper = copy;
				useShape = scaledWrapper;
			}
		}

		btTransform worldTrans;
		worldTrans.setIdentity();
		worldTrans.setOrigin(btVector3(p.x, p.y, p.z));
		worldTrans.setRotation(btQuaternion(
			p.rotation[0], p.rotation[1], p.rotation[2], p.rotation[3]));

		btDefaultMotionState* motion = new btDefaultMotionState(worldTrans);
		btRigidBody::btRigidBodyConstructionInfo info(0.0f, motion, useShape, btVector3(0, 0, 0));
		btRigidBody* body = new btRigidBody(info);
		body->setFriction(0.9f);
		body->setRestitution(0.0f);
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

		/* Keep scaled wrappers alive until Clear(). */
		if (scaledWrapper) {
			m_instanceShapes.push_back(scaledWrapper);
			body->setUserPointer(scaledWrapper);
		}

		m_dynamicsWorld->addRigidBody(
			body,
			btBroadphaseProxy::StaticFilter,
			btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);
		m_staticBodies.push_back(body);
		matched++;
	}

	printf("[Info] Bullet CollisionWorld: %d static bodies, %d unique shapes\n",
		matched, (int)m_shapes.size());
}

void CollisionWorld::Step(float dt)
{
	if (!m_dynamicsWorld)
		return;
	if (dt < 0.0f)
		dt = 0.0f;
	if (dt > 0.1f)
		dt = 0.1f;
	m_dynamicsWorld->stepSimulation(dt, 7, kFixedTimeStep);
}

void CollisionWorld::SetDebugDrawer(btIDebugDraw* drawer)
{
	if (m_dynamicsWorld)
		m_dynamicsWorld->setDebugDrawer(drawer);
}

void CollisionWorld::DebugDrawWorld()
{
	if (m_dynamicsWorld)
		m_dynamicsWorld->debugDrawWorld();
}

bool CollisionWorld::RayTestClosest(
	float x0, float y0, float z0,
	float x1, float y1, float z1,
	float* hitX, float* hitY, float* hitZ,
	ColVec3* hitNormal) const
{
	if (!m_dynamicsWorld)
		return false;

	btVector3 from(x0, y0, z0);
	btVector3 to(x1, y1, z1);
	btCollisionWorld::ClosestRayResultCallback cb(from, to);
	cb.m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
	m_dynamicsWorld->rayTest(from, to, cb);
	if (!cb.hasHit())
		return false;

	if (hitX) *hitX = cb.m_hitPointWorld.x();
	if (hitY) *hitY = cb.m_hitPointWorld.y();
	if (hitZ) *hitZ = cb.m_hitPointWorld.z();
	if (hitNormal) {
		hitNormal->x = cb.m_hitNormalWorld.x();
		hitNormal->y = cb.m_hitNormalWorld.y();
		hitNormal->z = cb.m_hitNormalWorld.z();
	}
	return true;
}

bool CollisionWorld::FindGroundY(float x, float y, float z, float* outY) const
{
	float hitY = 0.0f;
	if (!RayTestClosest(x, y, z, x, y - 2000.0f, z, nullptr, &hitY, nullptr, nullptr))
		return false;
	if (outY)
		*outY = hitY;
	return true;
}

bool CollisionWorld::ProbeFeet(float x, float y, float z, bool wasStanding, float* outPedY) const
{
	float startY = y + (wasStanding ? 0.25f : 0.05f);
	float endY = y - (wasStanding ? 1.25f : 0.55f);
	float hitY = 0.0f;
	if (!RayTestClosest(x, startY, z, x, endY, z, nullptr, &hitY, nullptr, nullptr))
		return false;
	if (outPedY)
		*outPedY = hitY + PED_FEET_OFFSET;
	return true;
}

bool CollisionWorld::CastDownLine(float x, float z, float startY, float endY, float* hitY, ColVec3* hitNormal) const
{
	float hy = 0.0f;
	ColVec3 n(0, 1, 0);
	if (!RayTestClosest(x, startY, z, x, endY, z, nullptr, &hy, nullptr, &n))
		return false;
	if (hitY)
		*hitY = hy;
	if (hitNormal)
		*hitNormal = n;
	return true;
}

void CollisionWorld::ResolvePedSpheres(float*, float*, float*) const
{
	/* Contacts handled by btKinematicCharacterController. */
}

void CollisionWorld::ResolveSpheres(
	const ColSphere*, int,
	float*, float*, float*,
	float*, float*, float*) const
{
	/* Contacts handled by btRaycastVehicle / chassis rigid body. */
}
