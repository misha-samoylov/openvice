#include "Camera.hpp"

#include <cmath>

void Camera::Init(float width, float height, float farPlane)
{
	m_cameraPosition = XMVectorSet(0.0f, 50.0f, 0.0f, 0.0f);
	m_cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	m_cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_cameraView = XMMatrixLookAtLH(m_cameraPosition, m_cameraTarget, m_cameraUp);
	m_cameraProjection = XMMatrixPerspectiveFovLH(0.4f * 3.14f, (float)width / height, 0.1f, farPlane);

	m_defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	m_defaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	m_cameraForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	m_cameraRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
}

void Camera::Cleanup()
{
}

void Camera::Update(float camPitch, float camYaw, float moveLeftRight, float moveBackForward)
{
	m_cameraRotationMatrix = XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0);
	m_cameraTarget = XMVector3TransformCoord(m_defaultForward, m_cameraRotationMatrix);
	m_cameraTarget = XMVector3Normalize(m_cameraTarget);

	m_cameraRight = XMVector3TransformCoord(m_defaultRight, m_cameraRotationMatrix);
	m_cameraForward = XMVector3TransformCoord(m_defaultForward, m_cameraRotationMatrix);
	m_cameraUp = XMVector3Cross(m_cameraForward, m_cameraRight);

	m_cameraPosition += moveLeftRight * m_cameraRight;
	m_cameraPosition += moveBackForward * m_cameraForward;

	m_cameraTarget = m_cameraPosition + m_cameraTarget;

	m_cameraView = XMMatrixLookAtLH(m_cameraPosition, m_cameraTarget, m_cameraUp);
}

void Camera::Follow(float targetX, float targetY, float targetZ,
	float yaw, float pitch, float distance, float heightOffset)
{
	if (pitch > 1.2f) pitch = 1.2f;
	if (pitch < -0.3f) pitch = -0.3f;

	XMVECTOR target = XMVectorSet(targetX, targetY + heightOffset, targetZ, 0.0f);

	float cp = cosf(pitch);
	float sp = sinf(pitch);
	float cy = cosf(yaw);
	float sy = sinf(yaw);

	/* Orbit behind the look direction (yaw=0 looks down +Z). */
	XMVECTOR offset = XMVectorSet(
		sy * cp * distance,
		-sp * distance,
		-cy * cp * distance,
		0.0f
	);

	m_cameraPosition = target + offset;
	m_cameraTarget = target;
	m_cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_cameraView = XMMatrixLookAtLH(m_cameraPosition, m_cameraTarget, m_cameraUp);

	m_cameraForward = XMVector3Normalize(m_cameraTarget - m_cameraPosition);
	m_cameraRight = XMVector3Normalize(XMVector3Cross(m_cameraUp, m_cameraForward));
}

XMMATRIX Camera::GetView()
{
	return m_cameraView;
}

XMVECTOR Camera::GetPosition()
{
	return m_cameraPosition;
}

XMMATRIX Camera::GetProjection()
{
	return m_cameraProjection;
}
