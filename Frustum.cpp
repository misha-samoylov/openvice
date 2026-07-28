#include "Frustum.h"

using namespace DirectX;

void Frustum::ConstructFrustum(float screenDepth, CXMMATRIX projectionMatrix, CXMMATRIX viewMatrix)
{
	XMMATRIX proj = projectionMatrix;

	float zMin = -XMVectorGetZ(projectionMatrix.r[3]) / XMVectorGetZ(projectionMatrix.r[2]);
	float r = screenDepth / (screenDepth - zMin);
	proj.r[2] = XMVectorSetZ(proj.r[2], r);
	proj.r[3] = XMVectorSetZ(proj.r[3], -r * zMin);

	XMMATRIX matrix = XMMatrixMultiply(viewMatrix, proj);
	matrix = XMMatrixTranspose(matrix);

	m_planes[0] = XMPlaneNormalize(matrix.r[3] + matrix.r[0]); /* left */
	m_planes[1] = XMPlaneNormalize(matrix.r[3] - matrix.r[0]); /* right */
	m_planes[2] = XMPlaneNormalize(matrix.r[3] + matrix.r[1]); /* bottom */
	m_planes[3] = XMPlaneNormalize(matrix.r[3] - matrix.r[1]); /* top */
	m_planes[4] = XMPlaneNormalize(matrix.r[3] + matrix.r[2]); /* near */
	m_planes[5] = XMPlaneNormalize(matrix.r[3] - matrix.r[2]); /* far */
}

bool Frustum::CheckPoint(float x, float y, float z) const
{
	XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
	for (int i = 0; i < 6; i++) {
		if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], position)) < 0.0f)
			return false;
	}
	return true;
}

bool Frustum::CheckCube(float xCenter, float yCenter, float zCenter, float size) const
{
	XMVECTOR position = XMVectorSet(xCenter, yCenter, zCenter, 1.0f);
	for (int i = 0; i < 6; i++) {
		if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], position)) < -size)
			return false;
	}
	return true;
}

bool Frustum::CheckSphere(float xCenter, float yCenter, float zCenter, float radius) const
{
	XMVECTOR position = XMVectorSet(xCenter, yCenter, zCenter, 1.0f);
	for (int i = 0; i < 6; i++) {
		if (XMVectorGetX(XMPlaneDotCoord(m_planes[i], position)) < -radius)
			return false;
	}
	return true;
}

bool Frustum::CheckRectangle(float xCenter, float yCenter, float zCenter, float xSize, float ySize, float zSize) const
{
	XMVECTOR position = XMVectorSet(xCenter, yCenter, zCenter, 1.0f);
	for (int i = 0; i < 6; i++) {
		float dotProduct = XMVectorGetX(XMPlaneDotCoord(m_planes[i], position));
		float x = fabsf(XMVectorGetX(m_planes[i])) * xSize;
		float y = fabsf(XMVectorGetY(m_planes[i])) * ySize;
		float z = fabsf(XMVectorGetZ(m_planes[i])) * zSize;
		if (dotProduct < -(x + y + z))
			return false;
	}
	return true;
}
