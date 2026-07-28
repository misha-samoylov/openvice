#pragma once

#include <DirectXMath.h>

using namespace DirectX;

/*
 * GTA uses Z-up: X east/west, Y north/south, Z up.
 * Engine / DirectX uses Y-up: remap (x,y,z) -> (x,z,y).
 * @see https://gtamods.com/wiki/Map_system
 */
namespace GtaCoords
{
	inline void ToEngine(float gx, float gy, float gz, float* ex, float* ey, float* ez)
	{
		*ex = gx;
		*ey = gz;
		*ez = gy;
	}

	inline XMFLOAT3 ToEngine(float gx, float gy, float gz)
	{
		return XMFLOAT3(gx, gz, gy);
	}

	/* Column-vector matrix that remaps GTA axes into engine space. */
	inline XMMATRIX ToEngineMatrix()
	{
		return XMMATRIX(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
	}

	/* Build a local GTA-space matrix from a RenderWare frame (3x3 + position). */
	inline XMMATRIX FrameLocalMatrix(const float rot9[9], const float pos[3])
	{
		XMMATRIX m = XMMatrixIdentity();
		m.r[0] = XMVectorSet(rot9[0], rot9[1], rot9[2], 0.0f);
		m.r[1] = XMVectorSet(rot9[3], rot9[4], rot9[5], 0.0f);
		m.r[2] = XMVectorSet(rot9[6], rot9[7], rot9[8], 0.0f);
		m.r[3] = XMVectorSet(pos[0], pos[1], pos[2], 1.0f);
		return m;
	}
}
