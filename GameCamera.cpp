#include "GameCamera.hpp"

void GameCamera::Init(float width, float height)
{
	m_cameraPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
	m_cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	m_cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_cameraView = XMMatrixLookAtLH(m_cameraPosition, m_cameraTarget, m_cameraUp);
	m_cameraProjection = XMMatrixPerspectiveFovLH(0.4f * 3.14f, (float)width / height, 0.1f, 50000.0f);

	m_defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	m_defaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	m_cameraForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	m_cameraRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
}

void GameCamera::Cleanup()
{
}

void GameCamera::Update(float camPitch, float camYaw, float moveLeftRight, float moveBackForward)
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

XMMATRIX GameCamera::GetView()
{
	return m_cameraView;
}

XMMATRIX GameCamera::GetProjection()
{
	return m_cameraProjection;
}