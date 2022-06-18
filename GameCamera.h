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
	XMVECTOR mCameraPosition;
	XMVECTOR mCameraTarget;
	XMVECTOR mCameraUp;

	XMMATRIX mCameraView;
	XMMATRIX mCameraProjection;

	XMVECTOR mDefaultForward;
	XMVECTOR mDefaultRight;
	XMVECTOR mCameraForward;
	XMVECTOR mCameraRight;

	XMMATRIX mCameraRotationMatrix;
};

