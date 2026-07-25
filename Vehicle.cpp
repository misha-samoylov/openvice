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
	m_wheelMeshes.clear();

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

	printf("[Info] Vehicle Cheetah ready (MI=%d) wheels=%d scale=%.2f\n",
		MI_CHEETAH, (int)m_wheelMeshes.size(), m_wheelScale);
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
	/* Textures live in generic.txd (already loaded into g_Textures from map path ideally).
	 * Also try loading wheel_sport materials from generic if present in IMG. */
	int txdId = img->GetFileIndexByName("generic.txd");
	std::vector<WheelTex> localTex;
	if (txdId >= 0) {
		char* buf = img->GetFileById((uint32_t)txdId);
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
	}

	int dffId = img->GetFileIndexByName("wheel_sport.dff");
	if (dffId < 0) {
		for (size_t i = 0; i < localTex.size(); i++)
			free(localTex[i].data);
		return false;
	}

	char* fileBuffer = img->GetFileById((uint32_t)dffId);
	Clump* clump = new Clump();
	clump->Read(fileBuffer);

	for (uint32_t gi = 0; gi < clump->m_numGeometries; gi++) {
		Geometry* geometry = clump->GetGeometryList()[gi];
		for (uint32_t si = 0; si < geometry->splits.size(); si++) {
			int v_count = geometry->vertexCount;
			float* meshVertexData = (float*)malloc(sizeof(float) * v_count * 5);
			for (int v = 0; v < v_count; v++) {
				float x = geometry->vertices[v * 3 + 0];
				float y = geometry->vertices[v * 3 + 1];
				float z = geometry->vertices[v * 3 + 2];
				float tx = 0.0f, ty = 0.0f;
				if (geometry->flags & FLAGS_TEXTURED) {
					tx = geometry->texCoords[0][v * 2 + 0];
					ty = geometry->texCoords[0][v * 2 + 1];
				}
				meshVertexData[v * 5 + 0] = x;
				meshVertexData[v * 5 + 1] = z;
				meshVertexData[v * 5 + 2] = y;
				meshVertexData[v * 5 + 3] = tx;
				meshVertexData[v * 5 + 4] = ty;
			}

			D3D_PRIMITIVE_TOPOLOGY topology =
				geometry->faceType == FACETYPE_STRIP
				? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			Mesh* mesh = new Mesh();
			mesh->Init(
				render,
				meshVertexData,
				v_count * 5,
				(unsigned int*)geometry->splits[si].indices,
				geometry->splits[si].m_numIndices,
				topology
			);
			free(meshVertexData);

			uint32_t matIndex = geometry->splits[si].matIndex;
			if (matIndex < geometry->m_numMaterials) {
				Material* material = geometry->materialList[matIndex];
				const char* matName = material->texture.name;
				for (size_t t = 0; t < localTex.size(); t++) {
					if (_stricmp(localTex[t].name, matName) == 0) {
						mesh->SetAlpha(localTex[t].isAlpha);
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
			}

			m_wheelMeshes.push_back(mesh);
		}
	}

	clump->Clear();
	delete clump;

	for (size_t i = 0; i < localTex.size(); i++)
		free(localTex[i].data);

	return !m_wheelMeshes.empty();
}

void Vehicle::Cleanup()
{
	for (size_t i = 0; i < m_wheelMeshes.size(); i++) {
		m_wheelMeshes[i]->Cleanup();
		delete m_wheelMeshes[i];
	}
	m_wheelMeshes.clear();
	m_model = nullptr;
	m_col = nullptr;
	m_world = nullptr;
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

void Vehicle::ProcessSuspension(float dt)
{
	m_wheelsOnGround = 0;
	if (!m_world)
		return;

	float c = cosf(m_heading);
	float s = sinf(m_heading);
	m_velY -= PED_GRAVITY * dt;

	float avgSupport = 0.0f;
	int supportCount = 0;

	for (int i = 0; i < WHEEL_COUNT; i++) {
		float lx = m_wheelLocal[i].x;
		float lz = m_wheelLocal[i].z;
		float wx = m_posX + (lx * c + lz * s);
		float wz = m_posZ + (-lx * s + lz * c);
		float topY = m_posY + m_suspUpper;
		float botY = m_posY - m_lineLength;

		float hitY = botY;
		ColVec3 normal(0, 1, 0);
		m_springRatio[i] = 1.0f;

		if (m_world->CastDownLine(wx, wz, topY, botY, &hitY, &normal)) {
			float lineSpan = topY - botY;
			if (lineSpan < 0.01f) lineSpan = 0.01f;
			float t = (topY - hitY) / lineSpan;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
			m_springRatio[i] = t;

			if (t < 1.0f) {
				m_wheelsOnGround++;
				float compression = 1.0f - t;
				if (compression > 0.0f) {
					if (normal.y < 0.0f)
						normal = normal * -1.0f;
					float impulse = PED_GRAVITY * m_mass * dt * m_suspForce * compression * 2.0f;
					float invMass = 1.0f / m_mass;
					m_velX += normal.x * impulse * invMass;
					m_velY += normal.y * impulse * invMass;
					m_velZ += normal.z * impulse * invMass;

					float vn = m_velX * normal.x + m_velY * normal.y + m_velZ * normal.z;
					float dampImp = -m_suspDamp * vn * m_mass * dt * 0.53f;
					m_velX += normal.x * dampImp * invMass;
					m_velY += normal.y * dampImp * invMass;
					m_velZ += normal.z * dampImp * invMass;

					avgSupport += hitY + m_heightAboveRoad;
					supportCount++;
				}
			}
		}
	}

	if (supportCount > 0) {
		float targetY = avgSupport / (float)supportCount;
		float err = targetY - m_posY;
		m_posY += err * Minf(1.0f, 8.0f * dt);
		if (m_velY < 0.0f && err > -0.05f)
			m_velY *= 0.3f;
	}
}

void Vehicle::ProcessDrive(float dt)
{
	float c = cosf(m_heading);
	float s = sinf(m_heading);
	float fx = -s;
	float fz = c;
	float rx = c;
	float rz = s;

	float fwdSpeed = m_velX * fx + m_velZ * fz;
	float rightSpeed = m_velX * rx + m_velZ * rz;

	if (m_wheelsOnGround > 0) {
		float thrust = 0.0f;
		if (fabsf(m_gasPedal) > 0.01f)
			thrust = m_gasPedal * m_engineAccel;
		if (m_brakePedal > 0.01f) {
			float br = m_brakePedal * m_brakeDecel;
			if (fwdSpeed > 0.1f) thrust -= br;
			else if (fwdSpeed < -0.1f) thrust += br;
			else {
				m_velX *= 0.85f;
				m_velZ *= 0.85f;
				thrust = 0.0f;
			}
		}

		m_velX += fx * thrust * dt;
		m_velZ += fz * thrust * dt;

		float grip = m_traction * 12.0f * dt;
		if (grip > 1.0f) grip = 1.0f;
		m_velX -= rx * rightSpeed * grip;
		m_velZ -= rz * rightSpeed * grip;

		float steerEff = m_steerAngle * Clampf(fabsf(fwdSpeed) / 12.0f, 0.0f, 1.0f);
		float turnSign = (fwdSpeed >= 0.0f) ? 1.0f : -1.0f;
		m_yawRate = steerEff * turnSign * 2.2f;
		if (m_brakePedal > 0.9f && fabsf(fwdSpeed) > 5.0f)
			m_yawRate *= 1.4f;

		m_velX -= fx * fwdSpeed * 0.4f * dt;
		m_velZ -= fz * fwdSpeed * 0.4f * dt;
	} else {
		m_yawRate *= 0.95f;
	}

	m_heading += m_yawRate * dt;

	float speed = sqrtf(m_velX * m_velX + m_velZ * m_velZ);
	if (speed > 0.01f) {
		float drag = 0.5f * speed * speed * (2.0f * 1.4f) / m_mass;
		float factor = 1.0f / (1.0f + drag * dt);
		m_velX *= factor;
		m_velZ *= factor;
	}

	fwdSpeed = m_velX * fx + m_velZ * fz;
	if (fabsf(fwdSpeed) > m_maxSpeed) {
		float scale = m_maxSpeed / fabsf(fwdSpeed);
		m_velX *= scale;
		m_velZ *= scale;
	}
}

void Vehicle::UpdateWheelRotation(float dt)
{
	/* re3 CVehicle::ProcessWheelRotation: ω = -dot(fwd, speed) / radius */
	float radius = m_wheelRadius;
	if (radius < 0.05f)
		radius = 0.35f;

	float hc = cosf(m_heading);
	float hs = sinf(m_heading);

	for (int i = 0; i < WHEEL_COUNT; i++) {
		float steer = m_wheelIsFront[i] ? m_steerAngle : 0.0f;
		/* Car-local forward after steer (GTA: -sin, cos on XY), engine XZ. */
		float lfx = -sinf(steer);
		float lfz = cosf(steer);
		float fx = lfx * hc + lfz * hs;
		float fz = -lfx * hs + lfz * hc;

		float contact = m_velX * fx + m_velZ * fz;
		float omega = -contact / radius;
		m_wheelRotation[i] += omega * dt;
	}
}

void Vehicle::BuildWorldSpheres(std::vector<ColSphere>& out) const
{
	out.clear();
	if (!m_col)
		return;

	float c = cosf(m_heading);
	float s = sinf(m_heading);
	size_t n = m_col->spheres.size();
	if (n > 16) n = 16;
	out.reserve(n);
	for (size_t i = 0; i < n; i++) {
		const ColSphere& ls = m_col->spheres[i];
		ColSphere ws;
		float lx = ls.center.x;
		float ly = ls.center.y;
		float lz = ls.center.z;
		ws.center.x = m_posX + (lx * c + lz * s);
		ws.center.y = m_posY + ly;
		ws.center.z = m_posZ + (-lx * s + lz * c);
		ws.radius = ls.radius;
		ws.surface = ls.surface;
		ws.piece = ls.piece;
		out.push_back(ws);
	}
}

void Vehicle::ProcessBodyCollision()
{
	if (!m_world)
		return;

	std::vector<ColSphere> spheres;
	BuildWorldSpheres(spheres);
	if (spheres.empty()) {
		ColSphere s;
		s.radius = 1.0f;
		s.surface = 0;
		s.piece = 0;
		s.center = ColVec3(m_posX, m_posY + 0.5f, m_posZ);
		spheres.push_back(s);
	}

	m_world->ResolveSpheres(
		spheres.data(), (int)spheres.size(),
		&m_posX, &m_posY, &m_posZ,
		&m_velX, &m_velY, &m_velZ);
}

void Vehicle::Update(float dt, float throttle, float steer, bool handbrake)
{
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.05f) dt = 0.05f;

	ProcessControlInputs(throttle, steer, handbrake);
	ProcessSuspension(dt);
	ProcessDrive(dt);
	UpdateWheelRotation(dt);

	m_posX += m_velX * dt;
	m_posY += m_velY * dt;
	m_posZ += m_velZ * dt;

	ProcessBodyCollision();

	if (m_posY < FALL_THROUGH_Y) {
		m_posY = 1000.0f;
		m_velY = 0.0f;
		if (!PlaceOnGround())
			m_posY = 5.0f + m_heightAboveRoad;
	}
}

void Vehicle::Render(DXRender* render, MeshRenderContext& ctx)
{
	if (!m_model)
		return;

	float half = m_heading * 0.5f;
	float qy = sinf(half);
	float qw = cosf(half);

	m_model->SetPosition(
		m_posX, m_posY, m_posZ,
		1.0f, 1.0f, 1.0f,
		0.0f, qy, 0.0f, qw);
	m_model->Render(render, ctx);

	if (m_wheelMeshes.empty())
		return;

	/*
	 * re3 CAutomobile::PreRender (engine-space remapped):
	 *   SetRotate(spin, 0, steer) on GTA XYZ → RotateX(spin)*RotateY(steer) here
	 *   Left wheels: -spin, PI+steer
	 *   Then Scale(wheelScale) and Translate(dummy pos)
	 */
	XMMATRIX chassis =
		XMMatrixRotationY(m_heading) *
		XMMatrixTranslation(m_posX, m_posY, m_posZ);

	for (int i = 0; i < WHEEL_COUNT; i++) {
		float spin = m_wheelRotation[i];
		float steerZ = m_wheelIsFront[i] ? m_steerAngle : 0.0f;
		if (m_wheelIsLeft[i]) {
			spin = -spin;
			steerZ = (float)M_PI + steerZ;
		}

		/* Suspension compresses wheel upward (engine Y). */
		float compress = (1.0f - m_springRatio[i]) * m_springLength;
		ColVec3 pos = m_wheelLocal[i];
		pos.y = m_wheelRestY[i] + compress;

		XMMATRIX local =
			XMMatrixRotationX(spin) *
			XMMatrixRotationY(steerZ) *
			XMMatrixScaling(m_wheelScale, m_wheelScale, m_wheelScale) *
			XMMatrixTranslation(pos.x, pos.y, pos.z);

		XMMATRIX world = XMMatrixMultiply(local, chassis);

		for (size_t m = 0; m < m_wheelMeshes.size(); m++) {
			m_wheelMeshes[m]->SetWorld(world);
			m_wheelMeshes[m]->Render(render, ctx);
		}
	}
}

XMVECTOR Vehicle::GetPosition() const
{
	return XMVectorSet(m_posX, m_posY, m_posZ, 0.0f);
}

void Vehicle::SetPosition(float x, float y, float z)
{
	m_posX = x;
	m_posY = y;
	m_posZ = z;
	m_velX = m_velY = m_velZ = 0.0f;
	m_yawRate = 0.0f;
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
	m_posY = groundY + m_heightAboveRoad + 0.4f;
	m_velX = m_velY = m_velZ = 0.0f;
	return true;
}
