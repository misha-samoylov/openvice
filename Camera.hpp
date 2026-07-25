#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
public:
	void Init(float width, float height, float farPlane = 800.0f);
	void Cleanup();
	void Update(float camPitch, float camYaw,
		float moveLeftRight, float moveBackForward);
	void Follow(float targetX, float targetY, float targetZ,
		float yaw, float pitch, float distance, float heightOffset);
	void SetPosition(float x, float y, float z);

	XMMATRIX GetView();
	XMMATRIX GetProjection();
	XMVECTOR GetPosition();

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
