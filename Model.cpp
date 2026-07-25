#include "Model.h"
#include <cmath>

void Model::UpdateCullExtent()
{
	if (!m_hasBounds) {
		m_cullExtent = 50.0f;
		return;
	}

	float centerLen = sqrtf(
		m_boundCenterX * m_boundCenterX +
		m_boundCenterY * m_boundCenterY +
		m_boundCenterZ * m_boundCenterZ
	);
	m_cullExtent = centerLen + m_boundRadius;
	if (m_cullExtent < 1.0f)
		m_cullExtent = 1.0f;
}

void Model::IncludeBoundingSphere(float x, float y, float z, float radius)
{
	if (radius < 0.0f)
		radius = 0.0f;

	if (!m_hasBounds) {
		m_boundCenterX = x;
		m_boundCenterY = y;
		m_boundCenterZ = z;
		m_boundRadius = radius;
		m_hasBounds = true;
		UpdateCullExtent();
		return;
	}

	float dx = x - m_boundCenterX;
	float dy = y - m_boundCenterY;
	float dz = z - m_boundCenterZ;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);

	if (dist + radius <= m_boundRadius) {
		UpdateCullExtent();
		return;
	}

	if (dist + m_boundRadius <= radius) {
		m_boundCenterX = x;
		m_boundCenterY = y;
		m_boundCenterZ = z;
		m_boundRadius = radius;
		UpdateCullExtent();
		return;
	}

	float newRadius = (m_boundRadius + radius + dist) * 0.5f;
	float k = (newRadius - m_boundRadius) / dist;
	m_boundCenterX += dx * k;
	m_boundCenterY += dy * k;
	m_boundCenterZ += dz * k;
	m_boundRadius = newRadius;
	UpdateCullExtent();
}

void Model::GetWorldCullSphere(
	float x, float y, float z,
	float sx, float sy, float sz,
	float* outX, float* outY, float* outZ, float* outRadius
) const
{
	float absSx = fabsf(sx);
	float absSy = fabsf(sy);
	float absSz = fabsf(sz);
	float maxScale = absSx;
	if (absSy > maxScale) maxScale = absSy;
	if (absSz > maxScale) maxScale = absSz;
	if (maxScale < 0.0001f)
		maxScale = 1.0f;

	*outX = x;
	*outY = y;
	*outZ = z;
	*outRadius = m_cullExtent * maxScale;
}

void Model::SetPosition(float x, float y, float z, float sx, float sy, float sz, float rx, float ry, float rz, float rr)
{
	XMMATRIX world =
		XMMatrixRotationQuaternion(XMVectorSet(rx, ry, rz, rr)) *
		XMMatrixScaling(sx, sy, sz) *
		XMMatrixTranslation(x, y, z);

	for (int i = 0; i < (int)m_pMeshes.size(); i++) {
		m_pMeshes[i]->SetWorld(world);
	}
}

void Model::Render(DXRender* pRender, MeshRenderContext& ctx, int alphaFilter)
{
	for (int i = 0; i < (int)m_pMeshes.size(); i++) {
		Mesh* mesh = m_pMeshes[i];
		bool isAlpha = mesh->GetAlpha();

		if (alphaFilter < 0) {
			if (isAlpha)
				continue;
		} else if (alphaFilter == 1) {
			if (!isAlpha || !mesh->IsAlphaCutout())
				continue;
		} else if (alphaFilter == 2) {
			if (!isAlpha || mesh->IsAlphaCutout())
				continue;
		}

		mesh->Render(pRender, ctx);
	}
}
