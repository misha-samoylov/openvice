#include "CollisionWorld.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <float.h>

using namespace DirectX;

namespace {

float Cross2D(float ax, float ay, float bx, float by)
{
	return ax * by - ay * bx;
}

} // namespace

void CollisionWorld::Clear()
{
	m_instances.clear();
	m_cells.clear();
	m_gridW = 0;
	m_gridH = 0;
	m_originX = 0;
	m_originZ = 0;
	m_cellSize = 50.0f;
}

ColVec3 CollisionWorld::TransformPoint(const XMMATRIX& m, const ColVec3& v)
{
	XMVECTOR r = XMVector3TransformCoord(XMVectorSet(v.x, v.y, v.z, 1.0f), m);
	return ColVec3(XMVectorGetX(r), XMVectorGetY(r), XMVectorGetZ(r));
}

ColVec3 CollisionWorld::TransformNormal(const XMMATRIX& m, const ColVec3& v)
{
	XMVECTOR r = XMVector3TransformNormal(XMVectorSet(v.x, v.y, v.z, 0.0f), m);
	ColVec3 n(XMVectorGetX(r), XMVectorGetY(r), XMVectorGetZ(r));
	return n.Normalized();
}

ColVec3 CollisionWorld::TransformPointInv(const XMMATRIX& inv, const ColVec3& v)
{
	return TransformPoint(inv, v);
}

void CollisionWorld::Build(COL* colStore, const std::vector<ColInstancePlacement>& placements)
{
	Clear();
	if (!colStore)
		return;

	m_instances.reserve(placements.size());
	int matched = 0;

	for (size_t i = 0; i < placements.size(); i++) {
		const ColInstancePlacement& p = placements[i];
		if (!p.model)
			continue;

		Instance inst;
		inst.model = p.model;
		inst.world =
			XMMatrixRotationQuaternion(XMVectorSet(p.rotation[0], p.rotation[1], p.rotation[2], p.rotation[3])) *
			XMMatrixScaling(p.scale[0], p.scale[1], p.scale[2]) *
			XMMatrixTranslation(p.x, p.y, p.z);

		XMVECTOR det;
		inst.invWorld = XMMatrixInverse(&det, inst.world);

		ColVec3 localCenter = p.model->boundSphere.center;
		inst.worldCenter = TransformPoint(inst.world, localCenter);

		float maxScale = fabsf(p.scale[0]);
		if (fabsf(p.scale[1]) > maxScale) maxScale = fabsf(p.scale[1]);
		if (fabsf(p.scale[2]) > maxScale) maxScale = fabsf(p.scale[2]);
		if (maxScale < 0.0001f) maxScale = 1.0f;
		inst.worldRadius = p.model->boundSphere.radius * maxScale;

		/* Conservative world AABB from 8 corners of local bound box. */
		const ColBox& lb = p.model->boundBox;
		ColVec3 corners[8] = {
			ColVec3(lb.min.x, lb.min.y, lb.min.z),
			ColVec3(lb.max.x, lb.min.y, lb.min.z),
			ColVec3(lb.min.x, lb.max.y, lb.min.z),
			ColVec3(lb.max.x, lb.max.y, lb.min.z),
			ColVec3(lb.min.x, lb.min.y, lb.max.z),
			ColVec3(lb.max.x, lb.min.y, lb.max.z),
			ColVec3(lb.min.x, lb.max.y, lb.max.z),
			ColVec3(lb.max.x, lb.max.y, lb.max.z)
		};
		inst.minX = FLT_MAX; inst.maxX = -FLT_MAX;
		inst.minZ = FLT_MAX; inst.maxZ = -FLT_MAX;
		for (int c = 0; c < 8; c++) {
			ColVec3 w = TransformPoint(inst.world, corners[c]);
			if (w.x < inst.minX) inst.minX = w.x;
			if (w.x > inst.maxX) inst.maxX = w.x;
			if (w.z < inst.minZ) inst.minZ = w.z;
			if (w.z > inst.maxZ) inst.maxZ = w.z;
		}

		m_instances.push_back(inst);
		matched++;
	}

	RebuildGrid();
	printf("[Info] CollisionWorld: %d instances, grid %dx%d\n",
		matched, m_gridW, m_gridH);
}

void CollisionWorld::RebuildGrid()
{
	m_cells.clear();
	m_gridW = 0;
	m_gridH = 0;
	if (m_instances.empty())
		return;

	float minX = FLT_MAX, maxX = -FLT_MAX, minZ = FLT_MAX, maxZ = -FLT_MAX;
	for (size_t i = 0; i < m_instances.size(); i++) {
		if (m_instances[i].minX < minX) minX = m_instances[i].minX;
		if (m_instances[i].maxX > maxX) maxX = m_instances[i].maxX;
		if (m_instances[i].minZ < minZ) minZ = m_instances[i].minZ;
		if (m_instances[i].maxZ > maxZ) maxZ = m_instances[i].maxZ;
	}

	m_cellSize = 50.0f;
	m_originX = minX - m_cellSize;
	m_originZ = minZ - m_cellSize;
	m_gridW = (int)ceilf((maxX - m_originX) / m_cellSize) + 1;
	m_gridH = (int)ceilf((maxZ - m_originZ) / m_cellSize) + 1;
	if (m_gridW < 1) m_gridW = 1;
	if (m_gridH < 1) m_gridH = 1;

	m_cells.resize((size_t)m_gridW * (size_t)m_gridH);

	for (size_t i = 0; i < m_instances.size(); i++) {
		const Instance& inst = m_instances[i];
		int x0 = (int)floorf((inst.minX - m_originX) / m_cellSize);
		int x1 = (int)floorf((inst.maxX - m_originX) / m_cellSize);
		int z0 = (int)floorf((inst.minZ - m_originZ) / m_cellSize);
		int z1 = (int)floorf((inst.maxZ - m_originZ) / m_cellSize);
		if (x0 < 0) x0 = 0;
		if (z0 < 0) z0 = 0;
		if (x1 >= m_gridW) x1 = m_gridW - 1;
		if (z1 >= m_gridH) z1 = m_gridH - 1;
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++)
				m_cells[(size_t)z * (size_t)m_gridW + (size_t)x].indices.push_back((int)i);
		}
	}
}

void CollisionWorld::QueryCells(float x, float z, float radius, std::vector<int>& out) const
{
	out.clear();
	if (m_cells.empty())
		return;

	int x0 = (int)floorf((x - radius - m_originX) / m_cellSize);
	int x1 = (int)floorf((x + radius - m_originX) / m_cellSize);
	int z0 = (int)floorf((z - radius - m_originZ) / m_cellSize);
	int z1 = (int)floorf((z + radius - m_originZ) / m_cellSize);
	if (x0 < 0) x0 = 0;
	if (z0 < 0) z0 = 0;
	if (x1 >= m_gridW) x1 = m_gridW - 1;
	if (z1 >= m_gridH) z1 = m_gridH - 1;

	static thread_local std::vector<uint8_t> seen;
	if (seen.size() < m_instances.size())
		seen.assign(m_instances.size(), 0);
	else
		memset(seen.data(), 0, m_instances.size());

	for (int cz = z0; cz <= z1; cz++) {
		for (int cx = x0; cx <= x1; cx++) {
			const Cell& cell = m_cells[(size_t)cz * (size_t)m_gridW + (size_t)cx];
			for (size_t i = 0; i < cell.indices.size(); i++) {
				int idx = cell.indices[i];
				if (seen[(size_t)idx])
					continue;
				seen[(size_t)idx] = 1;
				out.push_back(idx);
			}
		}
	}
}

bool CollisionWorld::TestLineBox(const ColVec3& p0, const ColVec3& p1, const ColBox& box)
{
	if (p0.x > box.min.x && p0.x < box.max.x &&
		p0.y > box.min.y && p0.y < box.max.y &&
		p0.z > box.min.z && p0.z < box.max.z)
		return true;
	if (p1.x > box.min.x && p1.x < box.max.x &&
		p1.y > box.min.y && p1.y < box.max.y &&
		p1.z > box.min.z && p1.z < box.max.z)
		return true;

	float t, x, y, z;
	if ((box.min.x - p1.x) * (box.min.x - p0.x) < 0.0f) {
		t = (box.min.x - p0.x) / (p1.x - p0.x);
		y = p0.y + (p1.y - p0.y) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (y > box.min.y && y < box.max.y && z > box.min.z && z < box.max.z)
			return true;
	}
	if ((p1.x - box.max.x) * (p0.x - box.max.x) < 0.0f) {
		t = (p0.x - box.max.x) / (p0.x - p1.x);
		y = p0.y + (p1.y - p0.y) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (y > box.min.y && y < box.max.y && z > box.min.z && z < box.max.z)
			return true;
	}
	if ((box.min.y - p0.y) * (box.min.y - p1.y) < 0.0f) {
		t = (box.min.y - p0.y) / (p1.y - p0.y);
		x = p0.x + (p1.x - p0.x) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (x > box.min.x && x < box.max.x && z > box.min.z && z < box.max.z)
			return true;
	}
	if ((p0.y - box.max.y) * (p1.y - box.max.y) < 0.0f) {
		t = (p0.y - box.max.y) / (p0.y - p1.y);
		x = p0.x + (p1.x - p0.x) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (x > box.min.x && x < box.max.x && z > box.min.z && z < box.max.z)
			return true;
	}
	if ((box.min.z - p0.z) * (box.min.z - p1.z) < 0.0f) {
		t = (box.min.z - p0.z) / (p1.z - p0.z);
		x = p0.x + (p1.x - p0.x) * t;
		y = p0.y + (p1.y - p0.y) * t;
		if (x > box.min.x && x < box.max.x && y > box.min.y && y < box.max.y)
			return true;
	}
	if ((p0.z - box.max.z) * (p1.z - box.max.z) < 0.0f) {
		t = (p0.z - box.max.z) / (p0.z - p1.z);
		x = p0.x + (p1.x - p0.x) * t;
		y = p0.y + (p1.y - p0.y) * t;
		if (x > box.min.x && x < box.max.x && y > box.min.y && y < box.max.y)
			return true;
	}
	return false;
}

bool CollisionWorld::ProcessLineSphere(
	const ColVec3& p0, const ColVec3& p1, const ColSphere& sphere, ColPoint& point, float& mindist)
{
	ColVec3 v01 = p1 - p0;
	ColVec3 v0c = sphere.center - p0;
	float linesq = v01.LengthSq();
	if (linesq < 1e-12f)
		return false;
	float projline = v01.Dot(v0c);
	float tansq = (v0c.LengthSq() - sphere.radius * sphere.radius) * linesq;
	float diffsq = projline * projline - tansq;
	if (diffsq < 0.0f)
		return false;
	float t = (projline - sqrtf(diffsq)) / linesq;
	if (t < 0.0f || t > 1.0f || t >= mindist)
		return false;
	point.point = p0 + v01 * t;
	point.normal = (point.point - sphere.center).Normalized();
	point.depth = 0.0f;
	point.surfaceB = sphere.surface;
	mindist = t;
	return true;
}

bool CollisionWorld::ProcessLineBox(
	const ColVec3& p0, const ColVec3& p1, const ColBox& box, ColPoint& point, float& mindist)
{
	float mint = 1.0f;
	ColVec3 normal;
	ColVec3 p;
	float t, x, y, z;

	if ((box.min.x - p1.x) * (box.min.x - p0.x) < 0.0f) {
		t = (box.min.x - p0.x) / (p1.x - p0.x);
		y = p0.y + (p1.y - p0.y) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (y > box.min.y && y < box.max.y && z > box.min.z && z < box.max.z && t < mint) {
			mint = t; p = ColVec3(box.min.x, y, z); normal = ColVec3(-1, 0, 0);
		}
	}
	if ((p1.x - box.max.x) * (p0.x - box.max.x) < 0.0f) {
		t = (p0.x - box.max.x) / (p0.x - p1.x);
		y = p0.y + (p1.y - p0.y) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (y > box.min.y && y < box.max.y && z > box.min.z && z < box.max.z && t < mint) {
			mint = t; p = ColVec3(box.max.x, y, z); normal = ColVec3(1, 0, 0);
		}
	}
	if ((box.min.y - p0.y) * (box.min.y - p1.y) < 0.0f) {
		t = (box.min.y - p0.y) / (p1.y - p0.y);
		x = p0.x + (p1.x - p0.x) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (x > box.min.x && x < box.max.x && z > box.min.z && z < box.max.z && t < mint) {
			mint = t; p = ColVec3(x, box.min.y, z); normal = ColVec3(0, -1, 0);
		}
	}
	if ((p0.y - box.max.y) * (p1.y - box.max.y) < 0.0f) {
		t = (p0.y - box.max.y) / (p0.y - p1.y);
		x = p0.x + (p1.x - p0.x) * t;
		z = p0.z + (p1.z - p0.z) * t;
		if (x > box.min.x && x < box.max.x && z > box.min.z && z < box.max.z && t < mint) {
			mint = t; p = ColVec3(x, box.max.y, z); normal = ColVec3(0, 1, 0);
		}
	}
	if ((box.min.z - p0.z) * (box.min.z - p1.z) < 0.0f) {
		t = (box.min.z - p0.z) / (p1.z - p0.z);
		x = p0.x + (p1.x - p0.x) * t;
		y = p0.y + (p1.y - p0.y) * t;
		if (x > box.min.x && x < box.max.x && y > box.min.y && y < box.max.y && t < mint) {
			mint = t; p = ColVec3(x, y, box.min.z); normal = ColVec3(0, 0, -1);
		}
	}
	if ((p0.z - box.max.z) * (p1.z - box.max.z) < 0.0f) {
		t = (p0.z - box.max.z) / (p0.z - p1.z);
		x = p0.x + (p1.x - p0.x) * t;
		y = p0.y + (p1.y - p0.y) * t;
		if (x > box.min.x && x < box.max.x && y > box.min.y && y < box.max.y && t < mint) {
			mint = t; p = ColVec3(x, y, box.max.z); normal = ColVec3(0, 0, 1);
		}
	}

	if (mint >= mindist)
		return false;
	point.point = p;
	point.normal = normal;
	point.depth = 0.0f;
	point.surfaceB = box.surface;
	mindist = mint;
	return true;
}

bool CollisionWorld::ProcessLineTriangle(
	const ColVec3& p0, const ColVec3& p1,
	const ColVec3& va, const ColVec3& vb, const ColVec3& vc,
	const ColTrianglePlane& plane,
	ColPoint& point, float& mindist)
{
	float d0 = plane.CalcPoint(p0);
	float d1 = plane.CalcPoint(p1);
	if (d0 * d1 > 0.0f)
		return false;

	ColVec3 dir = p1 - p0;
	float denom = dir.Dot(plane.normal);
	if (fabsf(denom) < 1e-12f)
		return false;

	float t = -d0 / denom;
	if (t < 0.0f || t >= mindist)
		return false;

	ColVec3 p = p0 + dir * t;

	ColVec3 normal = plane.normal;
	float u0, u1, u2, v0, v1, v2, pu, pv;
	switch (plane.dir) {
	case COL_DIR_X_POS:
		u0 = va.y; v0 = va.z; u1 = vc.y; v1 = vc.z; u2 = vb.y; v2 = vb.z; pu = p.y; pv = p.z;
		break;
	case COL_DIR_X_NEG:
		u0 = va.y; v0 = va.z; u1 = vb.y; v1 = vb.z; u2 = vc.y; v2 = vc.z; pu = p.y; pv = p.z;
		break;
	case COL_DIR_Y_POS:
		u0 = va.z; v0 = va.x; u1 = vc.z; v1 = vc.x; u2 = vb.z; v2 = vb.x; pu = p.z; pv = p.x;
		break;
	case COL_DIR_Y_NEG:
		u0 = va.z; v0 = va.x; u1 = vb.z; v1 = vb.x; u2 = vc.z; v2 = vc.x; pu = p.z; pv = p.x;
		break;
	case COL_DIR_Z_POS:
		u0 = va.x; v0 = va.y; u1 = vc.x; v1 = vc.y; u2 = vb.x; v2 = vb.y; pu = p.x; pv = p.y;
		break;
	default:
		u0 = va.x; v0 = va.y; u1 = vb.x; v1 = vb.y; u2 = vc.x; v2 = vc.y; pu = p.x; pv = p.y;
		break;
	}

	if (Cross2D(u1 - u0, v1 - v0, pu - u0, pv - v0) < 0.0f) return false;
	if (Cross2D(u2 - u0, v2 - v0, pu - u0, pv - v0) > 0.0f) return false;
	if (Cross2D(u2 - u1, v2 - v1, pu - u1, pv - v1) < 0.0f) return false;

	point.point = p;
	point.normal = normal;
	point.depth = 0.0f;
	mindist = t;
	return true;
}

bool CollisionWorld::ProcessVerticalLineInstance(
	const Instance& inst,
	const ColVec3& p0, const ColVec3& p1,
	ColPoint& best, float& mindist) const
{
	ColVec3 lp0 = TransformPointInv(inst.invWorld, p0);
	ColVec3 lp1 = TransformPointInv(inst.invWorld, p1);

	if (!TestLineBox(lp0, lp1, inst.model->boundBox))
		return false;

	bool hit = false;
	float coldist = mindist;
	ColPoint localHit;

	for (size_t i = 0; i < inst.model->spheres.size(); i++) {
		if (ProcessLineSphere(lp0, lp1, inst.model->spheres[i], localHit, coldist))
			hit = true;
	}
	for (size_t i = 0; i < inst.model->boxes.size(); i++) {
		if (ProcessLineBox(lp0, lp1, inst.model->boxes[i], localHit, coldist))
			hit = true;
	}
	for (size_t i = 0; i < inst.model->triangles.size(); i++) {
		const ColTriangle& tri = inst.model->triangles[i];
		if (ProcessLineTriangle(
			lp0, lp1,
			inst.model->vertices[tri.a],
			inst.model->vertices[tri.b],
			inst.model->vertices[tri.c],
			inst.model->planes[i],
			localHit, coldist)) {
			localHit.surfaceB = tri.surface;
			hit = true;
		}
	}

	if (!hit || coldist >= mindist)
		return false;

	best.point = TransformPoint(inst.world, localHit.point);
	best.normal = TransformNormal(inst.world, localHit.normal);
	best.depth = localHit.depth;
	best.surfaceB = localHit.surfaceB;
	mindist = coldist;
	return true;
}

bool CollisionWorld::FindGroundY(float x, float y, float z, float* outY) const
{
	ColVec3 p0(x, y, z);
	ColVec3 p1(x, -1000.0f, z);
	float mindist = 1.0f;
	ColPoint best;
	bool found = false;

	std::vector<int> candidates;
	QueryCells(x, z, 80.0f, candidates);

	for (size_t i = 0; i < candidates.size(); i++) {
		const Instance& inst = m_instances[(size_t)candidates[i]];
		float dx = x - inst.worldCenter.x;
		float dz = z - inst.worldCenter.z;
		float r = inst.worldRadius + 2.0f;
		if (dx * dx + dz * dz > r * r)
			continue;
		if (ProcessVerticalLineInstance(inst, p0, p1, best, mindist))
			found = true;
	}

	if (found && outY)
		*outY = best.point.y;
	return found;
}

bool CollisionWorld::ProbeFeet(float x, float y, float z, bool wasStanding, float* outPedY) const
{
	/* re3: line from ped pos down by FEET_OFFSET (+ gravity sink while standing). */
	float gravityEffect = wasStanding ? -0.15f : 0.0f;
	ColVec3 p0(x, y + (wasStanding ? -0.25f : 0.0f), z);
	ColVec3 p1(x, y - PED_FEET_OFFSET + gravityEffect, z);

	float mindist = 1.0f;
	ColPoint best;
	bool found = false;

	std::vector<int> candidates;
	QueryCells(x, z, 8.0f, candidates);

	for (size_t i = 0; i < candidates.size(); i++) {
		const Instance& inst = m_instances[(size_t)candidates[i]];
		float dx = x - inst.worldCenter.x;
		float dy = (y - 0.52f) - inst.worldCenter.y;
		float dz = z - inst.worldCenter.z;
		float r = inst.worldRadius + 0.52f - gravityEffect;
		if (dx * dx + dy * dy + dz * dz > r * r)
			continue;
		if (ProcessVerticalLineInstance(inst, p0, p1, best, mindist))
			found = true;
	}

	if (found && outPedY)
		*outPedY = best.point.y + PED_FEET_OFFSET;
	return found;
}

float CollisionWorld::DistToSegment(const ColVec3& a, const ColVec3& b, const ColVec3& p, ColVec3& closest)
{
	ColVec3 ab = b - a;
	float abLenSq = ab.LengthSq();
	if (abLenSq < 1e-12f) {
		closest = a;
		return (p - a).Length();
	}
	float t = (p - a).Dot(ab) / abLenSq;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	closest = a + ab * t;
	return (p - closest).Length();
}

bool CollisionWorld::ProcessSphereSphere(
	const ColSphere& a, const ColSphere& b, ColPoint& point, float& mindistsq)
{
	ColVec3 d = a.center - b.center;
	float distSq = d.LengthSq();
	float rad = a.radius + b.radius;
	if (distSq >= rad * rad || distSq >= mindistsq)
		return false;
	float dist = sqrtf(distSq);
	if (dist < 1e-6f) {
		point.normal = ColVec3(0, 1, 0);
		point.point = b.center;
		point.depth = a.radius;
	} else {
		point.normal = d * (1.0f / dist);
		point.point = b.center + point.normal * b.radius;
		point.depth = rad - dist;
	}
	point.surfaceB = b.surface;
	mindistsq = distSq;
	return true;
}

bool CollisionWorld::ProcessSphereBox(
	const ColSphere& sph, const ColBox& box, ColPoint& point, float& mindistsq)
{
	if (sph.center.x + sph.radius < box.min.x) return false;
	if (sph.center.x - sph.radius > box.max.x) return false;
	if (sph.center.y + sph.radius < box.min.y) return false;
	if (sph.center.y - sph.radius > box.max.y) return false;
	if (sph.center.z + sph.radius < box.min.z) return false;
	if (sph.center.z - sph.radius > box.max.z) return false;

	int xpos = sph.center.x < box.min.x ? 1 : sph.center.x > box.max.x ? 2 : 0;
	int ypos = sph.center.y < box.min.y ? 1 : sph.center.y > box.max.y ? 2 : 0;
	int zpos = sph.center.z < box.min.z ? 1 : sph.center.z > box.max.z ? 2 : 0;

	ColVec3 p, dist;
	if (xpos == 0 && ypos == 0 && zpos == 0) {
		p = (box.min + box.max) * 0.5f;
		dist = sph.center - p;
		float lensq = dist.LengthSq();
		if (lensq < 1e-12f)
			return false;
		if (lensq < mindistsq) {
			point.normal = dist * (1.0f / sqrtf(lensq));
			point.point = sph.center - point.normal;
			float dx = dist.x > 0.0f ? box.max.x - sph.center.x : sph.center.x - box.min.x;
			float dy = dist.y > 0.0f ? box.max.y - sph.center.y : sph.center.y - box.min.y;
			float dz = dist.z > 0.0f ? box.max.z - sph.center.z : sph.center.z - box.min.z;
			point.depth = (dx > dy && dx > dz) ? dx : (dy > dz ? dy : dz);
			point.surfaceB = box.surface;
			mindistsq = lensq;
			return true;
		}
	} else {
		p.x = xpos == 1 ? box.min.x : xpos == 2 ? box.max.x : sph.center.x;
		p.y = ypos == 1 ? box.min.y : ypos == 2 ? box.max.y : sph.center.y;
		p.z = zpos == 1 ? box.min.z : zpos == 2 ? box.max.z : sph.center.z;
		dist = sph.center - p;
		float lensq = dist.LengthSq();
		if (lensq < mindistsq) {
			float len = sqrtf(lensq);
			if (len < 1e-6f)
				return false;
			point.point = p;
			point.normal = dist * (1.0f / len);
			point.depth = sph.radius - len;
			point.surfaceB = box.surface;
			mindistsq = lensq;
			return true;
		}
	}
	return false;
}

bool CollisionWorld::ProcessSphereTriangle(
	const ColSphere& sph,
	const ColVec3& va, const ColVec3& vb, const ColVec3& vc,
	const ColTrianglePlane& plane,
	ColPoint& point, float& mindistsq)
{
	float planedist = plane.CalcPoint(sph.center);
	float distsq = planedist * planedist;
	if (fabsf(planedist) > sph.radius || distsq > mindistsq)
		return false;

	ColVec3 normal = plane.normal;
	ColVec3 vec2 = vb - va;
	float len = vec2.Length();
	if (len < 1e-8f)
		return false;
	vec2 = vec2 * (1.0f / len);
	ColVec3 vec1 = vec2.Cross(normal);

	ColVec3 vac = vc - va;
	ColVec3 vas = sph.center - va;
	float b0 = 0.0f, b1 = len;
	float c0 = vec1.Dot(vac), c1 = vec2.Dot(vac);
	float s0 = vec1.Dot(vas), s1 = vec2.Dot(vas);

	int insideAB = Cross2D(s0, s1, b0, b1) >= 0.0f;
	int insideAC = Cross2D(c0, c1, s0, s1) >= 0.0f;
	int insideBC = Cross2D(s0 - b0, s1 - b1, c0 - b0, c1 - b1) >= 0.0f;
	int testcase = insideAB + insideAC + insideBC;

	float dist = 0.0f;
	ColVec3 p;
	switch (testcase) {
	case 0:
		return false;
	case 1:
		if (insideAB) p = vc;
		else if (insideAC) p = vb;
		else p = va;
		dist = (sph.center - p).Length();
		break;
	case 2:
		if (!insideAB) dist = DistToSegment(va, vb, sph.center, p);
		else if (!insideAC) dist = DistToSegment(va, vc, sph.center, p);
		else dist = DistToSegment(vb, vc, sph.center, p);
		break;
	default:
		dist = fabsf(planedist);
		p = sph.center - normal * planedist;
		break;
	}

	if (dist >= sph.radius)
		return false;
	float dsq = dist * dist;
	if (dsq >= mindistsq)
		return false;

	point.depth = sph.radius - dist;
	if (dist < 1e-6f)
		point.normal = normal;
	else
		point.normal = (sph.center - p).Normalized();
	/* Prefer outward normal (away from triangle plane if ambiguous). */
	if (point.normal.Dot(normal) < 0.0f)
		point.normal = point.normal * -1.0f;
	point.point = p;
	mindistsq = dsq;
	return true;
}

bool CollisionWorld::ProcessSphereInstance(
	const Instance& inst,
	const ColSphere& worldSphere,
	ColPoint& best, float& mindistsq) const
{
	ColSphere local = worldSphere;
	local.center = TransformPointInv(inst.invWorld, worldSphere.center);
	/* Approximate uniform scale for radius. */
	XMVECTOR axisX = inst.world.r[0];
	float sx = XMVectorGetX(XMVector3Length(axisX));
	if (sx < 0.0001f) sx = 1.0f;
	local.radius = worldSphere.radius / sx;

	ColBox inflated = inst.model->boundBox;
	inflated.min = inflated.min - ColVec3(local.radius, local.radius, local.radius);
	inflated.max = inflated.max + ColVec3(local.radius, local.radius, local.radius);
	if (local.center.x < inflated.min.x || local.center.x > inflated.max.x ||
		local.center.y < inflated.min.y || local.center.y > inflated.max.y ||
		local.center.z < inflated.min.z || local.center.z > inflated.max.z)
		return false;

	bool hit = false;
	ColPoint localHit;
	float localMindistsq = mindistsq;

	for (size_t i = 0; i < inst.model->spheres.size(); i++) {
		if (ProcessSphereSphere(local, inst.model->spheres[i], localHit, localMindistsq))
			hit = true;
	}
	for (size_t i = 0; i < inst.model->boxes.size(); i++) {
		if (ProcessSphereBox(local, inst.model->boxes[i], localHit, localMindistsq))
			hit = true;
	}
	for (size_t i = 0; i < inst.model->triangles.size(); i++) {
		const ColTriangle& tri = inst.model->triangles[i];
		if (ProcessSphereTriangle(
			local,
			inst.model->vertices[tri.a],
			inst.model->vertices[tri.b],
			inst.model->vertices[tri.c],
			inst.model->planes[i],
			localHit, localMindistsq)) {
			localHit.surfaceB = tri.surface;
			hit = true;
		}
	}

	if (!hit)
		return false;

	best.point = TransformPoint(inst.world, localHit.point);
	best.normal = TransformNormal(inst.world, localHit.normal);
	best.depth = localHit.depth * sx;
	best.surfaceB = localHit.surfaceB;
	mindistsq = localMindistsq;
	return true;
}

void CollisionWorld::ResolvePedSpheres(float* x, float* y, float* z) const
{
	/* re3 TempColModels ped spheres (engine Y-up). */
	static const float offsets[3] = { -0.25f, 0.15f, 0.55f };

	std::vector<int> candidates;
	QueryCells(*x, *z, 4.0f, candidates);
	if (candidates.empty())
		return;

	for (int pass = 0; pass < 3; pass++) {
		for (int s = 0; s < 3; s++) {
			ColSphere sph;
			sph.center = ColVec3(*x, *y + offsets[s], *z);
			sph.radius = PED_SPHERE_RADIUS;
			sph.surface = 0;
			sph.piece = 0;

			ColPoint best;
			best.depth = 0.0f;
			float mindistsq = (sph.radius + 5.0f) * (sph.radius + 5.0f);
			bool any = false;

			for (size_t i = 0; i < candidates.size(); i++) {
				const Instance& inst = m_instances[(size_t)candidates[i]];
				float dx = sph.center.x - inst.worldCenter.x;
				float dy = sph.center.y - inst.worldCenter.y;
				float dz = sph.center.z - inst.worldCenter.z;
				float r = inst.worldRadius + sph.radius;
				if (dx * dx + dy * dy + dz * dz > r * r)
					continue;

				ColPoint hit;
				float dsq = mindistsq;
				if (ProcessSphereInstance(inst, sph, hit, dsq)) {
					if (hit.depth > best.depth) {
						best = hit;
						any = true;
					}
				}
			}

			if (!any || best.depth <= 0.0f)
				continue;

			/* Ped physics: cancel into-normal penetration (re3 bPedPhysics).
			 * Flatten near-vertical walls to horizontal push. */
			ColVec3 n = best.normal;
			if (fabsf(n.y) < 0.7f) {
				n.y = 0.0f;
				n = n.Normalized();
			}
			*x += n.x * best.depth;
			*y += n.y * best.depth;
			*z += n.z * best.depth;
		}
	}
}
