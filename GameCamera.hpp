#pragma once

#include <DirectXMath.h>

using namespace DirectX; /* DirectXMath */

class GameCamera
{
public:
	void Init(float width, float height);
	void Cleanup();
	void Update(float camPitch, float camYaw, float moveLeftRight, float moveBackForward);

	XMMATRIX getView();
	XMMATRIX getProjection();

private:
	XMVECTOR m_cameraPosition;
	XMVECTOR m_cameraTarget;
	XMVECTOR m_cameraUp;

	XMMATRIX m_cameraView;
	XMMATRIX m_cameraProjection;

	XMVECTOR m_defaultForward;
	XMVECTOR m_defaultRight;
	XMVECTOR m_cameraForward;
	XMVECTOR m_cameraRight;

	XMMATRIX m_cameraRotationMatrix;
};

