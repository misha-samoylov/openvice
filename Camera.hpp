#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
public:
	void Init(float width, float height);
	void Cleanup();
	void Update(float camPitch, float camYaw,
		float moveLeftRight, float moveBackForward);

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
