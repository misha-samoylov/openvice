#include "GameCamera.hpp"

void GameCamera::Init(float width, float height)
{
	mCameraPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
	mCameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	mCameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	mCameraView = XMMatrixLookAtLH(mCameraPosition, mCameraTarget, mCameraUp);
	mCameraProjection = XMMatrixPerspectiveFovLH(0.4f*3.14f, (float)width / height, 1.0f, 1000.0f);

	mDefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mDefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	mCameraForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	mCameraRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
}

void GameCamera::Cleanup()
{
	/* none */
}

void GameCamera::Update(float camPitch, float camYaw, float moveLeftRight, float moveBackForward)
{
	mCameraRotationMatrix = XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0);
	mCameraTarget = XMVector3TransformCoord(mDefaultForward, mCameraRotationMatrix);
	mCameraTarget = XMVector3Normalize(mCameraTarget);

	mCameraRight = XMVector3TransformCoord(mDefaultRight, mCameraRotationMatrix);
	mCameraForward = XMVector3TransformCoord(mDefaultForward, mCameraRotationMatrix);
	mCameraUp = XMVector3Cross(mCameraForward, mCameraRight);

	mCameraPosition += moveLeftRight * mCameraRight;
	mCameraPosition += moveBackForward * mCameraForward;

	mCameraTarget = mCameraPosition + mCameraTarget;

	mCameraView = XMMatrixLookAtLH(mCameraPosition, mCameraTarget, mCameraUp);
}

XMMATRIX GameCamera::getView()
{
	return mCameraView;
}

XMMATRIX GameCamera::getProjection()
{
	return mCameraProjection;
}