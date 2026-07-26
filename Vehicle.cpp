#include "Vehicle.h"
#include "CollisionWorld.h"
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

XMMATRIX GtaToEngineMat()
{
	return XMMATRIX(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

XMMATRIX FrameLocalGta(Frame* f)
{
	const float* r = f->GetRotationMatrix();
	const float* p = f->GetPosition();
	/* RW stores rows as right/up/at; build column-style for XM (row-vector). */
	XMMATRIX m = XMMatrixIdentity();
	m.r[0] = XMVectorSet(r[0], r[1], r[2], 0.0f);
	m.r[1] = XMVectorSet(r[3], r[4], r[5], 0.0f);
	m.r[2] = XMVectorSet(r[6], r[7], r[8], 0.0f);
	m.r[3] = XMVectorSet(p[0], p[1], p[2], 1.0f);
	return m;
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
	m_chassisBody = nullptr;
	m_chassisShape = nullptr;
	m_motionState = nullptr;
	m_vehicleRaycaster = nullptr;
	m_rayVehicle = nullptr;
	m_childShapes.clear();
	m_wheelMeshes.clear();
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
	m_engineAccel = 16.0f;
	m_brakeDecel = 11.1f;
	m_steerLock = 30.0f * (float)M_PI / 180.0f;
	m_suspForce = 2.2f;
	m_suspDamp = 0.20f;
	m_suspUpper = 0.40f;
	m_suspLower = 0.54f;
	m_wheelRadius = 0.5f * m_wheelScale;
	m_traction = 1.3f;
	m_maxSpeed = 230.0f * 1000.0f / 3600.0f;

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

	m_heightAboveRoad = 0.95f;

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

	printf("[Info] Vehicle Cheetah ready (MI=%d) wheels=%d scale=%.2f bullet=%s\n",
		MI_CHEETAH, (int)m_wheelMeshes.size(), m_wheelScale,
		m_rayVehicle ? "yes" : "no");
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
	 * Cheetah IDE wheel id 250 = frame "wheel_sport". Textures: wheels.txd /
	 * models/generic.txd / IMG generic.txd.
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

	int sportGeom = -1;
	int sportFrame = -1;
	for (uint32_t ai = 0; ai < atomics->GetNumAtomic(); ai++) {
		Atomic* atomic = atomics->GetAtomic((int)ai);
		int fi = atomic->GetFrameIndex();
		if (fi < 0 || fi >= frames->GetNumFrames())
			continue;
		const char* name = frames->GetFrame(fi)->GetName();
		if (!name || _stricmp(name, "wheel_sport") != 0)
			continue;
		sportGeom = atomic->GetGeometryIndex();
		sportFrame = fi;
		break;
	}

	if (sportGeom < 0 || (uint32_t)sportGeom >= clump->m_numGeometries) {
		printf("[Warn] Vehicle: wheel_sport atomic not found in wheels.dff\n");
		clump->Clear();
		delete clump;
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}

	Geometry* geometry = geometries[sportGeom];
	if (!geometry || !geometry->vertices || geometry->vertexCount == 0) {
		clump->Clear();
		delete clump;
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}

	/* Bake wheel frame LTM (usually identity under Group01) then GTA→engine. */
	XMMATRIX worldGta = FrameWorldGta(frames, sportFrame);
	int v_count = (int)geometry->vertexCount;
	float* meshVertexData = (float*)malloc(sizeof(float) * v_count * 5);
	for (int v = 0; v < v_count; v++) {
		float gx = geometry->vertices[v * 3 + 0];
		float gy = geometry->vertices[v * 3 + 1];
		float gz = geometry->vertices[v * 3 + 2];
		XMVECTOR w = XMVector3Transform(XMVectorSet(gx, gy, gz, 1.0f), worldGta);
		float wx = XMVectorGetX(w);
		float wy = XMVectorGetY(w);
		float wz = XMVectorGetZ(w);
		float tx = 0.0f, ty = 0.0f;
		if (geometry->flags & FLAGS_TEXTURED) {
			tx = geometry->texCoords[0][v * 2 + 0];
			ty = geometry->texCoords[0][v * 2 + 1];
		}
		meshVertexData[v * 5 + 0] = wx;
		meshVertexData[v * 5 + 1] = wz;
		meshVertexData[v * 5 + 2] = wy;
		meshVertexData[v * 5 + 3] = tx;
		meshVertexData[v * 5 + 4] = ty;
	}

	for (uint32_t si = 0; si < geometry->splits.size(); si++) {
		std::vector<uint32_t> triIndices;
		geometry->ExpandSplitToTriangles(si, triIndices);
		if (triIndices.empty())
			continue;

		Mesh* mesh = new Mesh();
		mesh->Init(
			render,
			meshVertexData,
			v_count * 5,
			triIndices.data(),
			(int)triIndices.size(),
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);

		uint32_t matIndex = geometry->splits[si].matIndex;
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
				break;
			}
		}

		m_wheelMeshes.push_back(mesh);
	}

	free(meshVertexData);
	clump->Clear();
	delete clump;

	for (size_t i = 0; i < localTex.size(); i++)
		free(localTex[i].data);

	printf("[Info] Vehicle: wheel_sport meshes=%d scale=%.2f\n",
		(int)m_wheelMeshes.size(), m_wheelScale);
	return !m_wheelMeshes.empty();
}

void Vehicle::Cleanup()
{
	DestroyBulletVehicle();
	for (size_t i = 0; i < m_wheelMeshes.size(); i++) {
		m_wheelMeshes[i]->Cleanup();
		delete m_wheelMeshes[i];
	}
	m_wheelMeshes.clear();
	m_model = nullptr;
	m_col = nullptr;
	m_world = nullptr;
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
			btSphereShape* sphere = new btSphereShape(s.radius);
			m_childShapes.push_back(sphere);
			btTransform local;
			local.setIdentity();
			local.setOrigin(btVector3(s.center.x, s.center.y, s.center.z));
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

	btVector3 inertia(0, 0, 0);
	m_chassisShape->calculateLocalInertia(m_mass, inertia);
	m_motionState = new btDefaultMotionState(start);
	btRigidBody::btRigidBodyConstructionInfo info(m_mass, m_motionState, m_chassisShape, inertia);
	m_chassisBody = new btRigidBody(info);
	m_chassisBody->setActivationState(DISABLE_DEACTIVATION);
	m_chassisBody->setDamping(0.05f, 0.4f);
	m_chassisBody->setFriction(0.8f);

	dyn->addRigidBody(m_chassisBody);

	m_vehicleRaycaster = new btDefaultVehicleRaycaster(dyn);
	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_suspensionStiffness = 20.0f + m_suspForce * 25.0f;
	tuning.m_suspensionCompression = 4.4f;
	tuning.m_suspensionDamping = 2.3f + m_suspDamp * 10.0f;
	tuning.m_maxSuspensionTravelCm = m_springLength * 100.0f;
	tuning.m_frictionSlip = 8.5f + m_traction * 2.0f;
	tuning.m_maxSuspensionForce = 6000.0f;

	m_rayVehicle = new btRaycastVehicle(tuning, m_chassisBody, m_vehicleRaycaster);
	m_rayVehicle->setCoordinateSystem(0, 1, 2); /* X right, Y up, Z forward */
	dyn->addAction(m_rayVehicle);

	btVector3 wheelDir(0, -1, 0);
	btVector3 wheelAxle(-1, 0, 0);
	float restLen = m_springLength;
	if (restLen < 0.08f)
		restLen = 0.08f;

	for (int i = 0; i < WHEEL_COUNT; i++) {
		btVector3 connection(
			m_wheelLocal[i].x,
			m_wheelLocal[i].y + m_suspUpper,
			m_wheelLocal[i].z);
		m_rayVehicle->addWheel(
			connection, wheelDir, wheelAxle,
			restLen, m_wheelRadius, tuning, m_wheelIsFront[i]);
	}

	for (int i = 0; i < m_rayVehicle->getNumWheels(); i++) {
		btWheelInfo& wi = m_rayVehicle->getWheelInfo(i);
		wi.m_suspensionStiffness = tuning.m_suspensionStiffness;
		wi.m_wheelsDampingRelaxation = tuning.m_suspensionDamping;
		wi.m_wheelsDampingCompression = tuning.m_suspensionCompression;
		wi.m_frictionSlip = tuning.m_frictionSlip;
		wi.m_rollInfluence = 0.1f;
	}

	printf("[Info] Vehicle: Bullet raycast vehicle created (wheels=%d)\n",
		m_rayVehicle->getNumWheels());
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
}

void Vehicle::Update(float dt, float throttle, float steer, bool handbrake)
{
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.05f) dt = 0.05f;

	SyncFromBullet();
	ProcessControlInputs(throttle, steer, handbrake);

	if (m_rayVehicle) {
		float engineForce = m_gasPedal * m_engineAccel * m_mass * 0.55f;
		float brakeForce = m_brakePedal * m_brakeDecel * m_mass * 0.35f;

		/* Cheetah is RWD. */
		m_rayVehicle->applyEngineForce(engineForce, WHEEL_RL);
		m_rayVehicle->applyEngineForce(engineForce, WHEEL_RR);
		m_rayVehicle->applyEngineForce(0.0f, WHEEL_FL);
		m_rayVehicle->applyEngineForce(0.0f, WHEEL_FR);

		m_rayVehicle->setSteeringValue(m_steerAngle, WHEEL_FL);
		m_rayVehicle->setSteeringValue(m_steerAngle, WHEEL_FR);

		m_rayVehicle->setBrake(brakeForce * 0.4f, WHEEL_FL);
		m_rayVehicle->setBrake(brakeForce * 0.4f, WHEEL_FR);
		m_rayVehicle->setBrake(brakeForce, WHEEL_RL);
		m_rayVehicle->setBrake(brakeForce, WHEEL_RR);

		/* Soft speed cap. */
		float speed = sqrtf(m_velX * m_velX + m_velZ * m_velZ);
		if (speed > m_maxSpeed && m_chassisBody) {
			float scale = m_maxSpeed / speed;
			btVector3 lv = m_chassisBody->getLinearVelocity();
			lv.setX(lv.x() * scale);
			lv.setZ(lv.z() * scale);
			m_chassisBody->setLinearVelocity(lv);
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
	XMMATRIX chassisXm =
		XMMatrixRotationQuaternion(XMVectorSet(qx, qy, qz, qw)) *
		XMMatrixTranslation(m_posX, m_posY, m_posZ);

	/* Vehicle body + wheels: follow global F1 wireframe (and per-vehicle flag). */
	render->SetVehicleRasterizer(m_wireframe || render->IsWireframe());

	m_model->SetPosition(
		m_posX, m_posY, m_posZ,
		1.0f, 1.0f, 1.0f,
		qx, qy, qz, qw);
	m_model->Render(render, ctx);

	if (!m_wheelMeshes.empty()) {
		/*
		 * Visual wheels stay in chassis-local space (re3 PreRender style).
		 * Bullet getWheelTransformWS uses a different axle/steer basis and
		 * drifted from the body when composed via OpenGL→XM conversion.
		 */
		for (int i = 0; i < WHEEL_COUNT; i++) {
			float spin = m_wheelRotation[i];
			float steerZ = m_wheelIsFront[i] ? m_steerAngle : 0.0f;
			if (m_wheelIsLeft[i]) {
				spin = -spin;
				steerZ = (float)M_PI + steerZ;
			}

			float compress = (1.0f - m_springRatio[i]) * m_springLength;
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
	return XMVectorSet(m_posX, m_posY, m_posZ, 0.0f);
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
	WarpChassis(m_posX, groundY + m_heightAboveRoad + 0.4f, m_posZ);
	return true;
}
