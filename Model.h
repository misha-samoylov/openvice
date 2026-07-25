#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string>

#include "Mesh.hpp"
#include "DXRender.hpp"
#include "Camera.hpp"

class Model
{
public:
	Model()
		: m_id(0)
		, m_hasAlpha(false)
		, m_hasBounds(false)
		, m_boundCenterX(0.0f)
		, m_boundCenterY(0.0f)
		, m_boundCenterZ(0.0f)
		, m_boundRadius(0.0f)
		, m_cullExtent(50.0f)
	{
	}

	void AddMesh(Mesh* pMesh) { m_pMeshes.push_back(pMesh); };
	std::vector<Mesh*>& GetMeshes() { return m_pMeshes; }

	void SetId(int id) { m_id = id; };
	int GetId() const { return m_id; };

	void SetAlpha(bool IsAlpha) { m_hasAlpha = IsAlpha; };
	bool IsAlpha() const { return m_hasAlpha; };

	void SetName(std::string name) { m_name = name; };
	const std::string& GetName() const { return m_name; }

	/* Local-space sphere already axis-remapped to engine coords (x, z, y). */
	void IncludeBoundingSphere(float x, float y, float z, float radius);

	/*
	 * Fast conservative world cull sphere:
	 * center = instance origin, radius covers rotated/scaled local bounds.
	 */
	void GetWorldCullSphere(
		float x, float y, float z,
		float sx, float sy, float sz,
		float* outX, float* outY, float* outZ, float* outRadius
	) const;

	void SetPosition(float x, float y, float z,
		float sx, float sy, float sz,
		float rx, float ry, float rz, float rr);

	void Render(DXRender* pRender, MeshRenderContext& ctx);

	void Cleanup()
	{
		for (int i = 0; i < (int)m_pMeshes.size(); i++) {
			m_pMeshes[i]->Cleanup();
			delete m_pMeshes[i];
		}
		m_pMeshes.clear();
	};

private:
	void UpdateCullExtent();

	int m_id;
	bool m_hasAlpha;
	std::string m_name;
	std::vector<Mesh*> m_pMeshes;

	bool m_hasBounds;
	float m_boundCenterX;
	float m_boundCenterY;
	float m_boundCenterZ;
	float m_boundRadius;
	float m_cullExtent;
};

#endif
