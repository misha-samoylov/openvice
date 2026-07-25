#pragma once

#include <vector>
#include <DirectXMath.h>

#include "collision/ColTypes.h"
#include "loaders/COL.hpp"

using namespace DirectX;

struct ColInstancePlacement {
	ColModel* model;
	float x, y, z;
	float scale[3];
	float rotation[4]; /* xyzw quaternion in engine space */
};

class CollisionWorld
{
public:
	void Clear();
	void Build(COL* colStore, const std::vector<ColInstancePlacement>& placements);

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
	 * Sphere vs world like CCollision::ProcessColModels for ped spheres.
	 * Resolves penetration by pushing position along contact normals (horizontal bias).
	 */
	void ResolvePedSpheres(float* x, float* y, float* z) const;

	/* General sphere resolve for vehicles (uses provided spheres in world space). */
	void ResolveSpheres(
		const ColSphere* spheres, int count,
		float* x, float* y, float* z,
		float* vx = nullptr, float* vy = nullptr, float* vz = nullptr) const;

	size_t GetInstanceCount() const { return m_instances.size(); }

private:
	struct Instance {
		ColModel* model;
		XMMATRIX world;
		XMMATRIX invWorld;
		ColVec3 worldCenter;
		float worldRadius;
		float minX, maxX, minZ, maxZ;
	};

	struct Cell {
		std::vector<int> indices;
	};

	void RebuildGrid();
	void QueryCells(float x, float z, float radius, std::vector<int>& out) const;

	bool ProcessVerticalLineInstance(
		const Instance& inst,
		const ColVec3& p0, const ColVec3& p1,
		ColPoint& best, float& mindist) const;

	bool ProcessSphereInstance(
		const Instance& inst,
		const ColSphere& sphere,
		ColPoint& best, float& mindistsq) const;

	static ColVec3 TransformPoint(const XMMATRIX& m, const ColVec3& v);
	static ColVec3 TransformNormal(const XMMATRIX& m, const ColVec3& v);
	static ColVec3 TransformPointInv(const XMMATRIX& inv, const ColVec3& v);

	static bool TestLineBox(const ColVec3& p0, const ColVec3& p1, const ColBox& box);
	static bool ProcessLineSphere(const ColVec3& p0, const ColVec3& p1, const ColSphere& sph, ColPoint& point, float& mindist);
	static bool ProcessLineBox(const ColVec3& p0, const ColVec3& p1, const ColBox& box, ColPoint& point, float& mindist);
	static bool ProcessLineTriangle(
		const ColVec3& p0, const ColVec3& p1,
		const ColVec3& va, const ColVec3& vb, const ColVec3& vc,
		const ColTrianglePlane& plane,
		ColPoint& point, float& mindist);
	static bool ProcessSphereBox(const ColSphere& sph, const ColBox& box, ColPoint& point, float& mindistsq);
	static bool ProcessSphereSphere(const ColSphere& a, const ColSphere& b, ColPoint& point, float& mindistsq);
	static bool ProcessSphereTriangle(
		const ColSphere& sph,
		const ColVec3& va, const ColVec3& vb, const ColVec3& vc,
		const ColTrianglePlane& plane,
		ColPoint& point, float& mindistsq);
	static float DistToSegment(const ColVec3& a, const ColVec3& b, const ColVec3& p, ColVec3& closest);

	std::vector<Instance> m_instances;
	std::vector<Cell> m_cells;
	int m_gridW;
	int m_gridH;
	float m_originX;
	float m_originZ;
	float m_cellSize;
};
