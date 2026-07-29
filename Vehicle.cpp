#include "Vehicle.h"
#include "CollisionWorld.h"
#include "core/GtaCoords.h"
#include "loaders/Clump.h"
#include "loaders/Geometry.h"
#include "renderware.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <algorithm>
#include <float.h>

#define __BT_DISABLE_SSE__
#include "btBulletDynamicsCommon.h"
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

float Minf(float a, float b) { return a < b ? a : b; }
float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Body origin (COM) sits this far below visual/COL center to resist tip-overs. */
const float kComDrop = 0.45f;

XMMATRIX FrameLocalGta(Frame* f)
{
	return GtaCoords::FrameLocalMatrix(f->GetRotationMatrix(), f->GetPosition());
}

XMMATRIX FrameWorldGta(FrameList* frames, int idx)
{
	if (idx < 0 || idx >= frames->GetNumFrames())
		return XMMatrixIdentity();
	Frame* f = frames->GetFrame(idx);
	XMMATRIX local = FrameLocalGta(f);
	int parent = f->GetParent();
	if (parent < 0)
		return local;
	/* row-vector: world = local * parentWorld */
	return XMMatrixMultiply(local, FrameWorldGta(frames, parent));
}

struct WheelTex {
	char name[24];
	uint8_t* data;
	size_t size;
	uint32_t width, height, dxt, depth;
	bool isAlpha;
};

} // namespace

bool Vehicle::Init(Model* model, ColModel* col, CollisionWorld* world, IMG* img, DXRender* render)
{
	m_model = model;
	m_col = col;
	m_world = world;
	m_render = render;
	m_chassisBody = nullptr;
	m_chassisShape = nullptr;
	m_motionState = nullptr;
	m_vehicleRaycaster = nullptr;
	m_rayVehicle = nullptr;
	m_childShapes.clear();
	m_wheelMeshes.clear();
	m_deformRest.clear();
	m_deformDirty = false;
	m_wireframe = false;

	m_posX = m_posY = m_posZ = 0.0f;
	m_velX = m_velY = m_velZ = 0.0f;
	m_heading = 0.0f;
	m_yawRate = 0.0f;
	m_gasPedal = 0.0f;
	m_brakePedal = 0.0f;
	m_steerAngle = 0.0f;
	m_steerInput = 0.0f;
	m_wheelsOnGround = 0;
	m_wheelScale = 0.7f; /* cheetah IDE field */

	m_mass = 1200.0f;
	m_engineAccel = 9.0f;
	m_brakeDecel = 5.0f;
	m_steerLock = 30.0f * (float)M_PI / 180.0f;
	m_suspForce = 2.2f;
	m_suspDamp = 0.28f;
	m_suspUpper = 0.40f;
	m_suspLower = 0.54f;
	m_wheelRadius = 0.5f * m_wheelScale;
	m_traction = 0.6f;
	m_maxSpeed = 200.0f * 1000.0f / 3600.0f;

	m_springLength = fabsf(m_suspUpper - m_suspLower);
	if (m_springLength < 0.05f)
		m_springLength = 0.14f;
	m_lineLength = m_springLength + m_wheelRadius;

	/* Defaults if dummies missing (engine local). */
	m_wheelLocal[WHEEL_FL] = ColVec3(-0.85f, 0.0f, 1.30f);
	m_wheelLocal[WHEEL_FR] = ColVec3( 0.85f, 0.0f, 1.30f);
	m_wheelLocal[WHEEL_RL] = ColVec3(-0.85f, 0.0f, -1.20f);
	m_wheelLocal[WHEEL_RR] = ColVec3( 0.85f, 0.0f, -1.20f);
	m_wheelIsLeft[WHEEL_FL] = true;  m_wheelIsFront[WHEEL_FL] = true;
	m_wheelIsLeft[WHEEL_FR] = false; m_wheelIsFront[WHEEL_FR] = true;
	m_wheelIsLeft[WHEEL_RL] = true;  m_wheelIsFront[WHEEL_RL] = false;
	m_wheelIsLeft[WHEEL_RR] = false; m_wheelIsFront[WHEEL_RR] = false;

	for (int i = 0; i < WHEEL_COUNT; i++) {
		m_springRatio[i] = 1.0f;
		m_wheelRotation[i] = 0.0f;
		m_wheelRestY[i] = m_wheelLocal[i].y;
	}

	/* Wheel hubs should rest at ~ground+radius after PlaceOnGround. */
	m_heightAboveRoad = 0.95f;
	m_rideHeight = 0.06f; /* small body lift above suspension hard-points */

	if (!m_model) {
		printf("[Error] Vehicle: no model\n");
		return false;
	}
	if (!m_col)
		printf("[Warn] Vehicle: no COL for cheetah — body collision limited\n");

	if (img && render) {
		LoadWheelDummies(img);
		if (!LoadWheelMeshes(img, render))
			printf("[Warn] Vehicle: wheel_sport meshes failed — driving without visible wheels\n");
	}

	CreateBulletVehicle();
	CaptureDeformRestPose();

	printf("[Info] Vehicle Cheetah ready (MI=%d) wheels=%d scale=%.2f bullet=%s deformMeshes=%d\n",
		MI_CHEETAH, (int)m_wheelMeshes.size(), m_wheelScale,
		m_rayVehicle ? "yes" : "no", (int)m_deformRest.size());
	return true;
}

bool Vehicle::LoadWheelDummies(IMG* img)
{
	int fileId = img->GetFileIndexByName("cheetah.dff");
	if (fileId < 0)
		return false;

	char* data = img->GetFileById((uint32_t)fileId);
	Clump* clump = new Clump();
	clump->Read(data);
	FrameList* frames = clump->GetFrameList();
	if (!frames) {
		clump->Clear();
		delete clump;
		return false;
	}

	struct DummyMap { const char* name; int idx; };
	DummyMap map[4] = {
		{ "wheel_lf_dummy", WHEEL_FL },
		{ "wheel_rf_dummy", WHEEL_FR },
		{ "wheel_lb_dummy", WHEEL_RL },
		{ "wheel_rb_dummy", WHEEL_RR },
	};

	int found = 0;
	for (int i = 0; i < frames->GetNumFrames(); i++) {
		Frame* f = frames->GetFrame(i);
		const char* name = f->GetName();
		if (!name)
			continue;
		for (int d = 0; d < 4; d++) {
			if (_stricmp(name, map[d].name) != 0)
				continue;
			XMMATRIX w = FrameWorldGta(frames, i);
			XMVECTOR t = w.r[3];
			float gx = XMVectorGetX(t);
			float gy = XMVectorGetY(t);
			float gz = XMVectorGetZ(t);
			/* GTA → engine local. */
			int wi = map[d].idx;
			m_wheelLocal[wi] = ColVec3(gx, gz, gy);
			m_wheelRestY[wi] = m_wheelLocal[wi].y;
			found++;
			printf("[Info] Vehicle: %s at engine (%.2f, %.2f, %.2f)\n",
				name, m_wheelLocal[wi].x, m_wheelLocal[wi].y, m_wheelLocal[wi].z);
		}
	}

	clump->Clear();
	delete clump;
	return found > 0;
}

bool Vehicle::LoadWheelMeshes(IMG* img, DXRender* render)
{
	/*
	 * re3/VC: wheel models live in models/generic/wheels.dff (not gta3.img).
	 * Cheetah IDE wheel id 250 = frame "wheel_sport" (geometry often on
	 * wheel_sport_l0). Textures: wheels.txd / models/generic.txd / IMG.
	 */
	static const char* kWheelsDff =
		"C:/Games/Grand Theft Auto Vice City/models/generic/wheels.dff";
	static const char* kWheelsTxdPaths[] = {
		"C:/Games/Grand Theft Auto Vice City/models/generic/wheels.txd",
		"C:/Games/Grand Theft Auto Vice City/models/generic.txd",
		nullptr
	};

	std::vector<WheelTex> localTex;

	auto appendTxdBuffer = [&](char* buf) {
		if (!buf)
			return;
		size_t offset = 0;
		TextureDictionary txd;
		txd.read(buf, &offset);
		for (size_t i = 0; i < txd.texList.size(); i++) {
			NativeTexture& t = txd.texList[i];
			if (t.dxtCompression == 0)
				t.convertTo32Bit();
			WheelTex wt;
			memset(&wt, 0, sizeof(wt));
			memcpy(wt.name, t.name, sizeof(wt.name));
			wt.size = t.dataSizes[0];
			wt.data = (uint8_t*)malloc(wt.size);
			memcpy(wt.data, t.texels[0], wt.size);
			wt.width = t.width[0];
			wt.height = t.height[0];
			wt.dxt = t.dxtCompression;
			wt.depth = t.depth;
			wt.isAlpha = t.IsAlpha;
			localTex.push_back(wt);
		}
	};

	for (int i = 0; kWheelsTxdPaths[i]; i++) {
		FILE* f = fopen(kWheelsTxdPaths[i], "rb");
		if (!f)
			continue;
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz > 0) {
			char* buf = (char*)malloc((size_t)sz);
			if (fread(buf, 1, (size_t)sz, f) == (size_t)sz)
				appendTxdBuffer(buf);
			free(buf);
		}
		fclose(f);
		if (!localTex.empty())
			break;
	}

	if (localTex.empty() && img) {
		int txdId = img->GetFileIndexByName("generic.txd");
		if (txdId >= 0)
			appendTxdBuffer(img->GetFileById((uint32_t)txdId));
	}

	FILE* dffFile = fopen(kWheelsDff, "rb");
	if (!dffFile) {
		printf("[Warn] Vehicle: cannot open %s\n", kWheelsDff);
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}
	fseek(dffFile, 0, SEEK_END);
	long dffSize = ftell(dffFile);
	fseek(dffFile, 0, SEEK_SET);
	if (dffSize <= 0) {
		fclose(dffFile);
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}
	char* fileBuffer = (char*)malloc((size_t)dffSize);
	if (fread(fileBuffer, 1, (size_t)dffSize, dffFile) != (size_t)dffSize) {
		free(fileBuffer);
		fclose(dffFile);
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}
	fclose(dffFile);

	Clump* clump = new Clump();
	clump->Read(fileBuffer);
	free(fileBuffer);

	FrameList* frames = clump->GetFrameList();
	AtomicList* atomics = clump->GetAtomicList();
	Geometry** geometries = clump->GetGeometryList();
	if (!frames || !atomics || !geometries) {
		clump->Clear();
		delete clump;
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}

	/* Prefer hi-LOD atomic; bare "wheel_sport" is a parent frame with no geom. */
	auto wheelPriority = [](const char* name) -> int {
		if (!name)
			return 100;
		if (_stricmp(name, "wheel_sport_l0") == 0)
			return 0;
		if (_stricmp(name, "wheel_sport") == 0)
			return 1;
		if (_strnicmp(name, "wheel_sport", 11) == 0) {
			if (strstr(name, "64") || strstr(name, "_l1"))
				return 50;
			return 10;
		}
		return 100;
	};

	int sportGeom = -1;
	int sportFrame = -1;
	int bestPri = 100;
	for (uint32_t ai = 0; ai < atomics->GetNumAtomic(); ai++) {
		Atomic* atomic = atomics->GetAtomic((int)ai);
		int fi = atomic->GetFrameIndex();
		if (fi < 0 || fi >= frames->GetNumFrames())
			continue;
		const char* name = frames->GetFrame(fi)->GetName();
		int pri = wheelPriority(name);
		if (pri >= bestPri)
			continue;
		int gi = atomic->GetGeometryIndex();
		if (gi < 0 || (uint32_t)gi >= clump->m_numGeometries)
			continue;
		Geometry* g = geometries[gi];
		if (!g || !g->vertices || g->vertexCount == 0)
			continue;
		bestPri = pri;
		sportGeom = gi;
		sportFrame = fi;
	}

	if (sportGeom < 0) {
		printf("[Warn] Vehicle: no wheel_sport atomic in wheels.dff (atomics=%u)\n",
			atomics->GetNumAtomic());
		for (uint32_t ai = 0; ai < atomics->GetNumAtomic() && ai < 32; ai++) {
			Atomic* atomic = atomics->GetAtomic((int)ai);
			int fi = atomic->GetFrameIndex();
			const char* name = (fi >= 0 && fi < frames->GetNumFrames())
				? frames->GetFrame(fi)->GetName() : "?";
			printf("  atomic[%u] frame='%s'\n", ai, name ? name : "(null)");
		}
		clump->Clear();
		delete clump;
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}

	Geometry* geometry = geometries[sportGeom];
	const char* usedName = frames->GetFrame(sportFrame)->GetName();
	printf("[Info] Vehicle: using wheel frame '%s' geom=%d verts=%u splits=%u tex=%d\n",
		usedName ? usedName : "?", sportGeom, geometry->vertexCount,
		(unsigned)geometry->splits.size(), (int)localTex.size());

	/*
	 * wheels.dff packs every wheel type in one clump with large frame offsets
	 * (wheel_sport LTM ~ Y=-6.7). Baking that LTM shoved meshes ~7m off the
	 * chassis. Geometry is already wheel-local — only remap GTA Z-up → Y-up.
	 */
	(void)sportFrame;
	int v_count = (int)geometry->vertexCount;
	float* meshVertexData = (float*)malloc(sizeof(float) * v_count * 5);
	for (int v = 0; v < v_count; v++) {
		float gx = geometry->vertices[v * 3 + 0];
		float gy = geometry->vertices[v * 3 + 1];
		float gz = geometry->vertices[v * 3 + 2];
		float tx = 0.0f, ty = 0.0f;
		if (geometry->flags & FLAGS_TEXTURED) {
			tx = geometry->texCoords[0][v * 2 + 0];
			ty = geometry->texCoords[0][v * 2 + 1];
		}
		float ex, ey, ez;
		GtaCoords::ToEngine(gx, gy, gz, &ex, &ey, &ez);
		meshVertexData[v * 5 + 0] = ex;
		meshVertexData[v * 5 + 1] = ey;
		meshVertexData[v * 5 + 2] = ez;
		meshVertexData[v * 5 + 3] = tx;
		meshVertexData[v * 5 + 4] = ty;
	}

	int meshesBuilt = 0;
	for (uint32_t si = 0; si < geometry->splits.size(); si++) {
		std::vector<uint32_t> triIndices;
		geometry->ExpandSplitToTriangles(si, triIndices);
		if (triIndices.empty()) {
			printf("[Warn] Vehicle: wheel split %u expanded to 0 tris (faceType=%u numIdx=%u)\n",
				si, geometry->faceType, geometry->splits[si].m_numIndices);
			continue;
		}

		Mesh* mesh = new Mesh();
		HRESULT hr = mesh->Init(
			render,
			meshVertexData,
			v_count * 5,
			triIndices.data(),
			(int)triIndices.size(),
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);
		if (FAILED(hr)) {
			printf("[Warn] Vehicle: wheel Mesh::Init failed hr=0x%08lx\n", hr);
			delete mesh;
			continue;
		}

		uint32_t matIndex = geometry->splits[si].matIndex;
		bool textured = false;
		if (matIndex < geometry->m_numMaterials) {
			Material* material = geometry->materialList[matIndex];
			const char* matName = material->texture.name;
			for (size_t t = 0; t < localTex.size(); t++) {
				if (_stricmp(localTex[t].name, matName) != 0)
					continue;
				mesh->SetAlpha(localTex[t].isAlpha);
				mesh->SetAlphaCutout(
					localTex[t].isAlpha &&
					localTex[t].dxt != 3 &&
					localTex[t].dxt != 4 &&
					localTex[t].dxt != 5
				);
				mesh->SetDataDDS(
					render,
					localTex[t].data,
					localTex[t].size,
					localTex[t].width,
					localTex[t].height,
					localTex[t].dxt,
					localTex[t].depth
				);
				textured = true;
				break;
			}
			if (!textured && matName && matName[0])
				printf("[Warn] Vehicle: wheel texture '%s' not in TXD\n", matName);
		}

		m_wheelMeshes.push_back(mesh);
		meshesBuilt++;
	}

	free(meshVertexData);
	clump->Clear();
	delete clump;

	for (size_t i = 0; i < localTex.size(); i++)
		free(localTex[i].data);

	printf("[Info] Vehicle: wheel meshes=%d scale=%.2f\n",
		meshesBuilt, m_wheelScale);
	return meshesBuilt > 0;
}

void Vehicle::Cleanup()
{
	DestroyBulletVehicle();
	for (size_t i = 0; i < m_wheelMeshes.size(); i++) {
		m_wheelMeshes[i]->Cleanup();
		delete m_wheelMeshes[i];
	}
	m_wheelMeshes.clear();
	m_deformRest.clear();
	m_deformDirty = false;
	m_model = nullptr;
	m_col = nullptr;
	m_world = nullptr;
	m_render = nullptr;
}

void Vehicle::SetCollisionWorld(CollisionWorld* world)
{
	if (m_world == world)
		return;
	DestroyBulletVehicle();
	m_world = world;
	if (m_world)
		CreateBulletVehicle();
}

void Vehicle::DestroyBulletVehicle()
{
	if (m_world && m_world->GetDynamicsWorld()) {
		btDiscreteDynamicsWorld* dyn = m_world->GetDynamicsWorld();
		if (m_rayVehicle)
			dyn->removeAction(m_rayVehicle);
		if (m_chassisBody)
			dyn->removeRigidBody(m_chassisBody);
	}

	if (m_rayVehicle) {
		delete m_rayVehicle;
		m_rayVehicle = nullptr;
	}
	if (m_vehicleRaycaster) {
		delete m_vehicleRaycaster;
		m_vehicleRaycaster = nullptr;
	}
	if (m_chassisBody) {
		delete m_chassisBody;
		m_chassisBody = nullptr;
	}
	if (m_motionState) {
		delete m_motionState;
		m_motionState = nullptr;
	}
	if (m_chassisShape) {
		delete m_chassisShape;
		m_chassisShape = nullptr;
	}
	for (size_t i = 0; i < m_childShapes.size(); i++)
		delete m_childShapes[i];
	m_childShapes.clear();
}

void Vehicle::CreateBulletVehicle()
{
	DestroyBulletVehicle();
	if (!m_world || !m_world->GetDynamicsWorld())
		return;

	btDiscreteDynamicsWorld* dyn = m_world->GetDynamicsWorld();
	m_chassisShape = new btCompoundShape();

	if (m_col && !m_col->spheres.empty()) {
		size_t n = m_col->spheres.size();
		if (n > 16) n = 16;
		for (size_t i = 0; i < n; i++) {
			const ColSphere& s = m_col->spheres[i];
			/* Slightly smaller than COL so wheels take the load at ride height. */
			float r = s.radius * 0.88f;
			if (r < 0.12f) r = 0.12f;
			btSphereShape* sphere = new btSphereShape(r);
			m_childShapes.push_back(sphere);
			btTransform local;
			local.setIdentity();
			/* Nudge up so bottoms clear the road when hubs are at ground+radius. */
			local.setOrigin(btVector3(s.center.x, s.center.y + m_wheelRadius * 0.25f, s.center.z));
			m_chassisShape->addChildShape(local, sphere);
		}
	} else if (m_col) {
		const ColBox& b = m_col->boundBox;
		btVector3 half(
			(b.max.x - b.min.x) * 0.5f,
			(b.max.y - b.min.y) * 0.5f,
			(b.max.z - b.min.z) * 0.5f);
		if (half.x() < 0.2f) half.setX(0.2f);
		if (half.y() < 0.2f) half.setY(0.2f);
		if (half.z() < 0.2f) half.setZ(0.2f);
		btBoxShape* box = new btBoxShape(half);
		m_childShapes.push_back(box);
		btTransform local;
		local.setIdentity();
		local.setOrigin(btVector3(
			(b.min.x + b.max.x) * 0.5f,
			(b.min.y + b.max.y) * 0.5f,
			(b.min.z + b.max.z) * 0.5f));
		m_chassisShape->addChildShape(local, box);
	} else {
		btBoxShape* box = new btBoxShape(btVector3(1.0f, 0.4f, 2.2f));
		m_childShapes.push_back(box);
		btTransform local;
		local.setIdentity();
		m_chassisShape->addChildShape(local, box);
	}

	btTransform start;
	start.setIdentity();
	start.setOrigin(btVector3(m_posX, m_posY, m_posZ));
	start.setRotation(btQuaternion(btVector3(0, 1, 0), m_heading));

	/*
	 * Raise COL relative to body origin so dynamics COM sits lower than the
	 * visual/collision center — stops brake/turn tip-overs.
	 */
	for (int i = 0; i < m_chassisShape->getNumChildShapes(); i++) {
		btTransform child = m_chassisShape->getChildTransform(i);
		child.getOrigin() += btVector3(0.0f, kComDrop, 0.0f);
		m_chassisShape->updateChildTransform(i, child, false);
	}
	m_chassisShape->recalculateLocalAabb();

	btVector3 inertia(0, 0, 0);
	m_chassisShape->calculateLocalInertia(m_mass, inertia);
	/* Resist pitch/roll; yaw stays more agile. */
	inertia.setX(inertia.x() * 3.5f);
	inertia.setY(inertia.y() * 1.8f);
	inertia.setZ(inertia.z() * 3.5f);
	m_motionState = new btDefaultMotionState(start);
	btRigidBody::btRigidBodyConstructionInfo info(m_mass, m_motionState, m_chassisShape, inertia);
	m_chassisBody = new btRigidBody(info);
	m_chassisBody->setActivationState(DISABLE_DEACTIVATION);
	m_chassisBody->setDamping(0.08f, 0.90f);
	m_chassisBody->setFriction(0.20f);
	m_chassisBody->setRestitution(0.0f);
	m_chassisBody->setCcdMotionThreshold(0.05f);
	m_chassisBody->setCcdSweptSphereRadius(0.4f);

	/*
	 * Must collide with static ground as a safety net — otherwise a missed
	 * wheel ray (COL seams while driving) drops the car through the map.
	 * Slightly smaller COL spheres so suspension, not the body, carries ride height.
	 */
	dyn->addRigidBody(m_chassisBody);

	m_vehicleRaycaster = new btDefaultVehicleRaycaster(dyn);
	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_suspensionStiffness = 28.0f + m_suspForce * 28.0f;
	tuning.m_suspensionCompression = 5.2f;
	tuning.m_suspensionDamping = 3.2f + m_suspDamp * 12.0f;
	tuning.m_maxSuspensionTravelCm = 28.0f;
	tuning.m_frictionSlip = 4.2f + m_traction;
	tuning.m_maxSuspensionForce = 9000.0f;

	m_rayVehicle = new btRaycastVehicle(tuning, m_chassisBody, m_vehicleRaycaster);
	m_rayVehicle->setCoordinateSystem(0, 1, 2); /* X right, Y up, Z forward */
	dyn->addAction(m_rayVehicle);

	btVector3 wheelDir(0, -1, 0);
	btVector3 wheelAxle(-1, 0, 0);
	/*
	 * Rest length from hard-point to hub. Long enough that rays keep contact
	 * over map seams; travel budget allows compression without losing the ray.
	 */
	float restLen = m_suspUpper + m_rideHeight + 0.12f;
	if (restLen < 0.35f)
		restLen = 0.35f;

	for (int i = 0; i < WHEEL_COUNT; i++) {
		btVector3 connection(
			m_wheelLocal[i].x,
			m_wheelLocal[i].y + restLen + kComDrop,
			m_wheelLocal[i].z);
		m_rayVehicle->addWheel(
			connection, wheelDir, wheelAxle,
			restLen, m_wheelRadius * 1.05f, tuning, m_wheelIsFront[i]);
	}

	for (int i = 0; i < m_rayVehicle->getNumWheels(); i++) {
		btWheelInfo& wi = m_rayVehicle->getWheelInfo(i);
		wi.m_suspensionStiffness = tuning.m_suspensionStiffness;
		wi.m_wheelsDampingRelaxation = tuning.m_suspensionDamping;
		wi.m_wheelsDampingCompression = tuning.m_suspensionCompression;
		wi.m_frictionSlip = tuning.m_frictionSlip;
		wi.m_rollInfluence = 0.02f;
		wi.m_maxSuspensionForce = tuning.m_maxSuspensionForce;
		wi.m_maxSuspensionTravelCm = tuning.m_maxSuspensionTravelCm;
	}

	printf("[Info] Vehicle: Bullet raycast vehicle created (wheels=%d rest=%.2f)\n",
		m_rayVehicle->getNumWheels(), restLen);
}

void Vehicle::ProcessControlInputs(float throttle, float steer, bool handbrake)
{
	if (throttle > 1.0f) throttle = 1.0f;
	if (throttle < -1.0f) throttle = -1.0f;
	if (steer > 1.0f) steer = 1.0f;
	if (steer < -1.0f) steer = -1.0f;

	m_steerInput += (steer - m_steerInput) * 0.35f;
	float s = m_steerInput;
	float s2 = (s < 0.0f ? -1.0f : 1.0f) * s * s;
	m_steerAngle = m_steerLock * s2;

	float fwdSpeed = m_velX * (-sinf(m_heading)) + m_velZ * cosf(m_heading);
	if (throttle * fwdSpeed < 0.0f && fabsf(fwdSpeed) > 0.5f) {
		m_gasPedal = 0.0f;
		m_brakePedal = fabsf(throttle);
	} else {
		m_gasPedal = throttle;
		m_brakePedal = 0.0f;
	}
	if (handbrake)
		m_brakePedal = 1.0f;
}

void Vehicle::SyncFromBullet()
{
	if (!m_chassisBody)
		return;

	btTransform t;
	if (m_motionState)
		m_motionState->getWorldTransform(t);
	else
		t = m_chassisBody->getWorldTransform();

	const btVector3& p = t.getOrigin();
	m_posX = p.x();
	m_posY = p.y();
	m_posZ = p.z();

	btVector3 fwd = m_rayVehicle ? m_rayVehicle->getForwardVector() : t.getBasis().getColumn(2);
	m_heading = atan2f(-fwd.x(), fwd.z());

	btVector3 lv = m_chassisBody->getLinearVelocity();
	m_velX = lv.x();
	m_velY = lv.y();
	m_velZ = lv.z();

	m_wheelsOnGround = 0;
	if (m_rayVehicle) {
		for (int i = 0; i < m_rayVehicle->getNumWheels(); i++) {
			m_rayVehicle->updateWheelTransform(i, true);
			const btWheelInfo& wi = m_rayVehicle->getWheelInfo(i);
			m_wheelRotation[i] = wi.m_rotation;
			float travel = wi.m_raycastInfo.m_suspensionLength;
			float maxTravel = m_springLength > 0.01f ? m_springLength : 0.14f;
			m_springRatio[i] = Clampf(travel / maxTravel, 0.0f, 1.0f);
			if (wi.m_raycastInfo.m_isInContact)
				m_wheelsOnGround++;
		}
	}
}

void Vehicle::WarpChassis(float x, float y, float z)
{
	m_posX = x;
	m_posY = y;
	m_posZ = z;
	m_velX = m_velY = m_velZ = 0.0f;
	m_yawRate = 0.0f;

	if (!m_chassisBody)
		return;

	btTransform t;
	t.setIdentity();
	t.setOrigin(btVector3(x, y, z));
	t.setRotation(btQuaternion(btVector3(0, 1, 0), m_heading));
	m_chassisBody->setWorldTransform(t);
	if (m_motionState)
		m_motionState->setWorldTransform(t);
	m_chassisBody->setLinearVelocity(btVector3(0, 0, 0));
	m_chassisBody->setAngularVelocity(btVector3(0, 0, 0));
	m_chassisBody->clearForces();
	if (m_rayVehicle)
		m_rayVehicle->resetSuspension();
}

void Vehicle::SyncPhysics()
{
	SyncFromBullet();
	ProcessCollisionDeform();
}

void Vehicle::CaptureDeformRestPose()
{
	m_deformRest.clear();
	m_deformDirty = false;
	if (!m_model)
		return;

	std::vector<Mesh*>& meshes = m_model->GetMeshes();
	m_deformRest.resize(meshes.size());
	for (size_t i = 0; i < meshes.size(); i++) {
		if (meshes[i])
			m_deformRest[i] = meshes[i]->GetVertexData();
	}
}

void Vehicle::ApplyDent(
	float localX, float localY, float localZ,
	float dirX, float dirY, float dirZ, float strength)
{
	if (!m_model || m_deformRest.empty() || strength <= 0.0f)
		return;

	float dirLen = sqrtf(dirX * dirX + dirY * dirY + dirZ * dirZ);
	if (dirLen < 1e-4f)
		return;
	dirX /= dirLen;
	dirY /= dirLen;
	dirZ /= dirLen;

	/* Soft radius + max dent depth scale with impact strength. */
	const float radius = 0.55f + strength * 0.35f;
	const float radiusSq = radius * radius;
	const float maxDent = 0.28f;
	const float push = Minf(0.18f, strength * 0.09f);

	std::vector<Mesh*>& meshes = m_model->GetMeshes();
	const size_t meshCount = meshes.size() < m_deformRest.size()
		? meshes.size() : m_deformRest.size();

	for (size_t mi = 0; mi < meshCount; mi++) {
		Mesh* mesh = meshes[mi];
		if (!mesh)
			continue;

		std::vector<float>& verts = mesh->GetVertexData();
		const std::vector<float>& rest = m_deformRest[mi];
		if (verts.size() != rest.size() || verts.size() < 5)
			continue;

		const int vCount = (int)verts.size() / 5;
		bool changed = false;
		for (int v = 0; v < vCount; v++) {
			const size_t o = (size_t)v * 5;
			float x = verts[o + 0];
			float y = verts[o + 1];
			float z = verts[o + 2];

			float dx = x - localX;
			float dy = y - localY;
			float dz = z - localZ;
			float distSq = dx * dx + dy * dy + dz * dz;
			if (distSq > radiusSq)
				continue;

			float dist = sqrtf(distSq);
			float t = 1.0f - dist / radius;
			float falloff = t * t;

			/* Prefer crumple along impact; nudge toward body center slightly. */
			float toCx = -x * 0.15f;
			float toCy = -y * 0.05f;
			float toCz = -z * 0.15f;
			float px = (dirX + toCx) * push * falloff;
			float py = (dirY + toCy) * push * falloff;
			float pz = (dirZ + toCz) * push * falloff;

			float nx = x + px;
			float ny = y + py;
			float nz = z + pz;

			float ox = rest[o + 0];
			float oy = rest[o + 1];
			float oz = rest[o + 2];
			float odx = nx - ox;
			float ody = ny - oy;
			float odz = nz - oz;
			float od = sqrtf(odx * odx + ody * ody + odz * odz);
			if (od > maxDent) {
				float s = maxDent / od;
				nx = ox + odx * s;
				ny = oy + ody * s;
				nz = oz + odz * s;
			}

			if (nx != x || ny != y || nz != z) {
				verts[o + 0] = nx;
				verts[o + 1] = ny;
				verts[o + 2] = nz;
				changed = true;
			}
		}

		if (changed)
			m_deformDirty = true;
	}
}

void Vehicle::ProcessCollisionDeform()
{
	if (!m_chassisBody || !m_world || !m_world->GetDynamicsWorld() || !m_model)
		return;
	if (m_deformRest.empty())
		return;

	btDiscreteDynamicsWorld* dyn = m_world->GetDynamicsWorld();
	btDispatcher* dispatcher = dyn->getDispatcher();
	if (!dispatcher)
		return;

	btTransform chassisWorld;
	if (m_motionState)
		m_motionState->getWorldTransform(chassisWorld);
	else
		chassisWorld = m_chassisBody->getWorldTransform();
	btTransform chassisInv = chassisWorld.inverse();

	const float impulseThreshold = 180.0f;

	const int numManifolds = dispatcher->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
		if (!manifold)
			continue;

		const btCollisionObject* obA = manifold->getBody0();
		const btCollisionObject* obB = manifold->getBody1();
		const bool aIsChassis = (obA == m_chassisBody);
		const bool bIsChassis = (obB == m_chassisBody);
		if (!aIsChassis && !bIsChassis)
			continue;

		const int nContacts = manifold->getNumContacts();
		for (int c = 0; c < nContacts; c++) {
			const btManifoldPoint& pt = manifold->getContactPoint(c);
			float impulse = pt.getAppliedImpulse();
			if (impulse < impulseThreshold)
				continue;

			/*
			 * m_normalWorldOnB points from B toward A. Force on chassis:
			 * chassis==A → +n, chassis==B → -n. Dent vertices along that push.
			 */
			btVector3 n = pt.m_normalWorldOnB;
			if (bIsChassis)
				n = -n;

			/* Skip soft underside scrapes (mostly +Y push while grounded). */
			if (n.y() > 0.75f && impulse < impulseThreshold * 2.5f)
				continue;

			btVector3 hitWorld = aIsChassis ? pt.getPositionWorldOnA() : pt.getPositionWorldOnB();
			btVector3 hitLocal = chassisInv * hitWorld;
			/* Visual mesh is drawn kComDrop above physics COM. */
			hitLocal.setY(hitLocal.y() - kComDrop);

			btVector3 dirLocal = chassisInv.getBasis() * n;

			float strength = Clampf((impulse - impulseThreshold) / 900.0f, 0.15f, 1.6f);
			ApplyDent(
				hitLocal.x(), hitLocal.y(), hitLocal.z(),
				dirLocal.x(), dirLocal.y(), dirLocal.z(),
				strength);
		}
	}

	if (m_deformDirty && m_render) {
		std::vector<Mesh*>& meshes = m_model->GetMeshes();
		for (size_t i = 0; i < meshes.size(); i++) {
			if (meshes[i])
				meshes[i]->UploadVertices(m_render);
		}
		m_deformDirty = false;
	}
}

void Vehicle::Update(float dt, float throttle, float steer, bool handbrake)
{
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.05f) dt = 0.05f;

	SyncFromBullet();
	ProcessControlInputs(throttle, steer, handbrake);

	if (m_rayVehicle) {
		float engineForce = m_gasPedal * m_engineAccel * m_mass * 0.55f;
		float brakeForce = m_brakePedal * m_brakeDecel * m_mass * 0.12f;

		/* Cheetah is RWD. */
		m_rayVehicle->applyEngineForce(engineForce, WHEEL_RL);
		m_rayVehicle->applyEngineForce(engineForce, WHEEL_RR);
		m_rayVehicle->applyEngineForce(0.0f, WHEEL_FL);
		m_rayVehicle->applyEngineForce(0.0f, WHEEL_FR);

		m_rayVehicle->setSteeringValue(m_steerAngle, WHEEL_FL);
		m_rayVehicle->setSteeringValue(m_steerAngle, WHEEL_FR);

		/* Front-biased brakes — rear-heavy bias lifts the rear under hard stop. */
		m_rayVehicle->setBrake(brakeForce, WHEEL_FL);
		m_rayVehicle->setBrake(brakeForce, WHEEL_FR);
		m_rayVehicle->setBrake(brakeForce * 0.30f, WHEEL_RL);
		m_rayVehicle->setBrake(brakeForce * 0.30f, WHEEL_RR);

		if (m_chassisBody) {
			/* Soft speed cap. */
			float speed = sqrtf(m_velX * m_velX + m_velZ * m_velZ);
			if (speed > m_maxSpeed) {
				float scale = m_maxSpeed / speed;
				btVector3 lv = m_chassisBody->getLinearVelocity();
				lv.setX(lv.x() * scale);
				lv.setZ(lv.z() * scale);
				m_chassisBody->setLinearVelocity(lv);
			}

			/* Cap local pitch/roll so sticky wheel forces can't cartwheel the chassis. */
			btTransform wt = m_chassisBody->getWorldTransform();
			btVector3 avWorld = m_chassisBody->getAngularVelocity();
			btVector3 avLocal = wt.getBasis().transpose() * avWorld;
			const float maxPitchRoll = 1.4f;
			const float maxYaw = 2.6f;
			if (avLocal.x() > maxPitchRoll) avLocal.setX(maxPitchRoll);
			if (avLocal.x() < -maxPitchRoll) avLocal.setX(-maxPitchRoll);
			if (avLocal.z() > maxPitchRoll) avLocal.setZ(maxPitchRoll);
			if (avLocal.z() < -maxPitchRoll) avLocal.setZ(-maxPitchRoll);
			if (avLocal.y() > maxYaw) avLocal.setY(maxYaw);
			if (avLocal.y() < -maxYaw) avLocal.setY(-maxYaw);
			m_chassisBody->setAngularVelocity(wt.getBasis() * avLocal);
		}
	}

	if (m_posY < FALL_THROUGH_Y) {
		WarpChassis(m_posX, 1000.0f, m_posZ);
		if (!PlaceOnGround())
			WarpChassis(m_posX, 5.0f + m_heightAboveRoad, m_posZ);
	}
}

void Vehicle::Render(DXRender* render, MeshRenderContext& ctx)
{
	if (!m_model)
		return;

	float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

	if (m_chassisBody) {
		btTransform t;
		if (m_motionState)
			m_motionState->getWorldTransform(t);
		else
			t = m_chassisBody->getWorldTransform();
		btQuaternion q = t.getRotation();
		qx = (float)q.x();
		qy = (float)q.y();
		qz = (float)q.z();
		qw = (float)q.w();
	} else {
		float half = m_heading * 0.5f;
		qy = sinf(half);
		qw = cosf(half);
	}

	/* Same chassis matrix as Model::SetPosition — keep wheels glued to the body. */
	XMVECTOR comLift = XMVector3Rotate(XMVectorSet(0.0f, kComDrop, 0.0f, 0.0f), XMVectorSet(qx, qy, qz, qw));
	float drawX = m_posX + XMVectorGetX(comLift);
	float drawY = m_posY + XMVectorGetY(comLift);
	float drawZ = m_posZ + XMVectorGetZ(comLift);

	XMMATRIX chassisXm =
		XMMatrixRotationQuaternion(XMVectorSet(qx, qy, qz, qw)) *
		XMMatrixTranslation(drawX, drawY, drawZ);

	/* Vehicle body + wheels: follow global F1 wireframe (and per-vehicle flag). */
	render->SetVehicleRasterizer(m_wireframe || render->IsWireframe());

	m_model->SetPosition(
		drawX, drawY, drawZ,
		1.0f, 1.0f, 1.0f,
		qx, qy, qz, qw);
	m_model->Render(render, ctx);

	if (!m_wheelMeshes.empty()) {
		/* Model may leave skinned/other bindings — force Mesh pipeline rebind. */
		ctx.ClearBindings();
		render->SetVehicleRasterizer(m_wireframe || render->IsWireframe());

		/*
		 * Chassis-local wheels (re3 PreRender). Suspension compress from Bullet
		 * spring ratio; keep a sane range so wheels stay visible.
		 */
		for (int i = 0; i < WHEEL_COUNT; i++) {
			float spin = m_wheelRotation[i];
			float steerZ = m_wheelIsFront[i] ? m_steerAngle : 0.0f;
			if (m_wheelIsLeft[i]) {
				spin = -spin;
				steerZ = (float)M_PI + steerZ;
			}

			float ratio = Clampf(m_springRatio[i], 0.25f, 1.0f);
			float compress = (1.0f - ratio) * m_springLength;
			ColVec3 pos = m_wheelLocal[i];
			pos.y = m_wheelRestY[i] + compress;

			XMMATRIX local =
				XMMatrixRotationX(spin) *
				XMMatrixRotationY(steerZ) *
				XMMatrixScaling(m_wheelScale, m_wheelScale, m_wheelScale) *
				XMMatrixTranslation(pos.x, pos.y, pos.z);

			XMMATRIX world = XMMatrixMultiply(local, chassisXm);

			for (size_t m = 0; m < m_wheelMeshes.size(); m++) {
				m_wheelMeshes[m]->SetWorld(world);
				m_wheelMeshes[m]->Render(render, ctx);
			}
		}
	}

	render->ApplyRasterizerState();
}

XMVECTOR Vehicle::GetPosition() const
{
	/* Visual center sits kComDrop above physics COM. */
	if (m_chassisBody) {
		btTransform t;
		if (m_motionState)
			m_motionState->getWorldTransform(t);
		else
			t = m_chassisBody->getWorldTransform();
		btVector3 up = t.getBasis().getColumn(1) * kComDrop;
		return XMVectorSet(m_posX + up.x(), m_posY + up.y(), m_posZ + up.z(), 0.0f);
	}
	return XMVectorSet(m_posX, m_posY + kComDrop, m_posZ, 0.0f);
}

void Vehicle::SetPosition(float x, float y, float z)
{
	WarpChassis(x, y, z);
}

void Vehicle::SetHeading(float heading)
{
	m_heading = heading;
	WarpChassis(m_posX, m_posY, m_posZ);
}

bool Vehicle::PlaceOnGround()
{
	if (!m_world)
		return false;
	float groundY = 0.0f;
	if (!m_world->FindGroundY(m_posX, m_posY + 5.0f, m_posZ, &groundY)) {
		if (!m_world->FindGroundY(m_posX, 1000.0f, m_posZ, &groundY))
			return false;
	}
	/*
	 * Chassis origin so wheel hubs (dummies) sit at ground + tyre radius.
	 * Suspension rays then hit the road; chassis COL ignores static ground.
	 */
	float hubY = 0.0f;
	for (int i = 0; i < WHEEL_COUNT; i++)
		hubY += m_wheelLocal[i].y;
	hubY *= 0.25f;
	float y = groundY + m_wheelRadius - hubY + 0.12f - kComDrop;
	WarpChassis(m_posX, y, m_posZ);
	return true;
}
