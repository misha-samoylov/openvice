#pragma once

#include <stdint.h>
#include <vector>
#include <cmath>

#include "core/GtaCoords.h"

/* Collision primitives — layout mirrors re3/miami CCol* types.
 * Stored in engine space (Y-up): GTA (x,y,z) -> engine (x,z,y). */

struct ColVec3 {
	float x, y, z;

	ColVec3() : x(0), y(0), z(0) {}
	ColVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	ColVec3 operator+(const ColVec3& o) const { return ColVec3(x + o.x, y + o.y, z + o.z); }
	ColVec3 operator-(const ColVec3& o) const { return ColVec3(x - o.x, y - o.y, z - o.z); }
	ColVec3 operator*(float s) const { return ColVec3(x * s, y * s, z * s); }
	float Dot(const ColVec3& o) const { return x * o.x + y * o.y + z * o.z; }
	ColVec3 Cross(const ColVec3& o) const {
		return ColVec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
	}
	float LengthSq() const { return x * x + y * y + z * z; }
	float Length() const { return sqrtf(LengthSq()); }
	ColVec3 Normalized() const {
		float len = Length();
		if (len < 1e-8f) return ColVec3(0, 1, 0);
		return (*this) * (1.0f / len);
	}
};

inline ColVec3 GtaToEngineVec(float gx, float gy, float gz)
{
	float ex, ey, ez;
	GtaCoords::ToEngine(gx, gy, gz, &ex, &ey, &ez);
	return ColVec3(ex, ey, ez);
}

struct ColSphere {
	ColVec3 center;
	float radius;
	uint8_t surface;
	uint8_t piece;
};

struct ColBox {
	ColVec3 min;
	ColVec3 max;
	uint8_t surface;
	uint8_t piece;
};

struct ColTriangle {
	uint16_t a, b, c;
	uint8_t surface;
};

enum ColPlaneDir {
	COL_DIR_X_POS = 0,
	COL_DIR_X_NEG,
	COL_DIR_Y_POS,
	COL_DIR_Y_NEG,
	COL_DIR_Z_POS,
	COL_DIR_Z_NEG
};

struct ColTrianglePlane {
	ColVec3 normal;
	float dist;
	uint8_t dir;

	void Set(const ColVec3& va, const ColVec3& vb, const ColVec3& vc)
	{
		normal = (vc - va).Cross(vb - va).Normalized();
		dist = normal.Dot(va);
		float ax = fabsf(normal.x), ay = fabsf(normal.y), az = fabsf(normal.z);
		if (ax > ay && ax > az)
			dir = normal.x < 0.0f ? (uint8_t)COL_DIR_X_NEG : (uint8_t)COL_DIR_X_POS;
		else if (ay > az)
			dir = normal.y < 0.0f ? (uint8_t)COL_DIR_Y_NEG : (uint8_t)COL_DIR_Y_POS;
		else
			dir = normal.z < 0.0f ? (uint8_t)COL_DIR_Z_NEG : (uint8_t)COL_DIR_Z_POS;
	}

	float CalcPoint(const ColVec3& v) const { return normal.Dot(v) - dist; }
};

struct ColPoint {
	ColVec3 point;
	ColVec3 normal;
	float depth;
	uint8_t surfaceB;
};

struct ColModel {
	char name[24];
	ColSphere boundSphere;
	ColBox boundBox;
	std::vector<ColSphere> spheres;
	std::vector<ColBox> boxes;
	std::vector<ColVec3> vertices;
	std::vector<ColTriangle> triangles;
	std::vector<ColTrianglePlane> planes;

	void CalculatePlanes()
	{
		planes.resize(triangles.size());
		for (size_t i = 0; i < triangles.size(); i++) {
			const ColTriangle& t = triangles[i];
			planes[i].Set(vertices[t.a], vertices[t.b], vertices[t.c]);
		}
	}
};

/* re3 Ped.h FEET_OFFSET — ped origin sits this far above the ground. */
static const float PED_FEET_OFFSET = 1.04f;
/* re3 TempColModels::ms_colModelPed1 sphere radius. */
static const float PED_SPHERE_RADIUS = 0.35f;
/* Gravity: re3 GRAVITY(0.008) * 50^2 → SI-ish units/s^2 with dt in seconds. */
static const float PED_GRAVITY = 20.0f;
/* re3 ApplyMoveForce(0,0,8.5) / mass(70) * 50 → units/s. */
static const float PED_JUMP_SPEED = (8.5f / 70.0f) * 50.0f;
/* Below this engine Y, entity is considered fallen through the map. */
static const float FALL_THROUGH_Y = -50.0f;
