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
	void AddMesh(Mesh* mesh) { m_meshes.push_back(mesh); };
	std::vector<Mesh*> GetMeshes() { return m_meshes; }

	void SetId(int id) { m_id = id; };
	int GetId() { return m_id; };

	void SetAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; };
	bool hasAlpha() { return m_hasAlpha; };

	void SetName(std::string name) { m_name = name; };

	void SetPosition(float  x, float  y, float  z, float  sx, float   sy, float  sz, float  rx, float  ry, float  rz, float rr);

	void Render(DXRender* render, Camera* camera);

private:
	int m_id;
	bool m_hasAlpha;
	std::string m_name;
	std::vector<Mesh*> m_meshes;
};

#endif
