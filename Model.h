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
	void AddMesh(Mesh* pMesh) { m_pMeshes.push_back(pMesh); };
	std::vector<Mesh*> GetMeshes() { return m_pMeshes; }

	void SetId(int id) { m_id = id; };
	int GetId() { return m_id; };

	void SetAlpha(bool IsAlpha) { m_hasAlpha = IsAlpha; };
	bool IsAlpha() { return m_hasAlpha; };

	void SetName(std::string name) { m_name = name; };

	void SetPosition(float x, float y, float z, 
		float sx, float sy, float sz, 
		float rx, float ry, float rz, float rr);

	void Render(DXRender* pRender, Camera* pCamera);

	void Cleanup()
	{
		for (int i = 0; i < m_pMeshes.size(); i++) {
			m_pMeshes[i]->Cleanup();
			delete m_pMeshes[i];
		}
	};

private:
	int m_id;
	bool m_hasAlpha;
	std::string m_name;
	std::vector<Mesh*> m_pMeshes;
};

#endif
