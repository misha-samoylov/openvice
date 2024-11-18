#include "Model.h"

void Model::SetPosition(float  x, float  y, float  z, float  sx, float   sy, float  sz, float  rx, float  ry, float  rz, float rr)
{
	for (int i = 0; i < m_meshes.size(); i++) {
		m_meshes[i]->SetPosition(x, y, z, sx, sy, sz, rx, ry, rz, rr);
	}
}

void Model::Render(DXRender* render, Camera* camera)
{
	for (int i = 0; i < m_meshes.size(); i++) {
		m_meshes[i]->Render(render, camera);
	}
}