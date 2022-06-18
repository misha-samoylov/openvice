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
	XMVECTOR camPosition;
	XMVECTOR camTarget;
	XMVECTOR camUp;

	XMMATRIX camView;
	XMMATRIX camProjection;

	XMVECTOR DefaultForward;
	XMVECTOR DefaultRight;
	XMVECTOR camForward;
	XMVECTOR camRight;

	XMMATRIX camRotationMatrix;
};

