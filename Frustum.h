#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Frustum
{
public:
	void ConstructFrustum(float screenDepth, CXMMATRIX projectionMatrix, CXMMATRIX viewMatrix);

	bool CheckPoint(float x, float y, float z) const;
	bool CheckCube(float xCenter, float yCenter, float zCenter, float size) const;
	bool CheckSphere(float xCenter, float yCenter, float zCenter, float radius) const;
	bool CheckRectangle(float xCenter, float yCenter, float zCenter, float xSize, float ySize, float zSize) const;

private:
	XMVECTOR m_planes[6];
};
