#include "world/Scene.h"
#include "assets/DffLoader.h"
#include "core/GameConfig.h"
#include "ShadowMap.h"

#include <stdio.h>
#include <cmath>
#include <algorithm>

void Scene::BuildFromAssets(AssetRegistry& assets, int worldHour)
{
	assets.RebuildIdIndex();
	m_opaque.clear();
	m_alpha.clear();
	m_opaque.reserve(65536);
	m_alpha.reserve(8192);

	for (size_t i = 0; i < assets.IplFiles().size(); i++) {
		IPL* ipl = assets.IplFiles()[i].get();
		int count = ipl->GetCountObjects();
		for (int j = 0; j < count; j++) {
			mapItem objectInfo = ipl->GetItem(j);

			/* Outdoor world + VIS_EVERYWHERE (13). Skip other interiors. */
			if (objectInfo.interior != INTERIOR_EXTERIOR
				&& objectInfo.interior != INTERIOR_EVERYWHERE)
				continue;

			if (DffLoader::IsLodModelName(objectInfo.modelName))
				continue;

			Model* model = assets.FindModelById(objectInfo.id);
			if (!model)
				continue;
			if (!model->IsVisibleAtHour(worldHour))
				continue;

			SceneInstance inst;
			inst.model = model;
			inst.x = objectInfo.x;
			inst.y = objectInfo.y;
			inst.z = objectInfo.z;
			inst.scale[0] = objectInfo.scale[0];
			inst.scale[1] = objectInfo.scale[1];
			inst.scale[2] = objectInfo.scale[2];
			inst.rotation[0] = objectInfo.rotation[0];
			inst.rotation[1] = objectInfo.rotation[1];
			inst.rotation[2] = objectInfo.rotation[2];
			inst.rotation[3] = objectInfo.rotation[3];

			if (model->IsAlpha())
				m_alpha.push_back(inst);
			else
				m_opaque.push_back(inst);
		}
	}

	printf("[Info] Scene instances: opaque=%u alpha=%u models=%u (hour=%d, timed hidden)\n",
		(unsigned)m_opaque.size(),
		(unsigned)m_alpha.size(),
		(unsigned)assets.Models().size(),
		worldHour);
}

bool Scene::IsVisible(Model* model, const SceneInstance& inst, const Frustum& frustum)
{
	float cx, cy, cz, radius;
	model->GetWorldCullSphere(
		inst.x, inst.y, inst.z,
		inst.scale[0], inst.scale[1], inst.scale[2],
		&cx, &cy, &cz, &radius
	);
	return frustum.CheckSphere(cx, cy, cz, radius);
}

bool Scene::InShadowRange(
	Model* model, const SceneInstance& inst,
	float focusX, float focusY, float focusZ, float range)
{
	float cx, cy, cz, radius;
	model->GetWorldCullSphere(
		inst.x, inst.y, inst.z,
		inst.scale[0], inst.scale[1], inst.scale[2],
		&cx, &cy, &cz, &radius
	);
	float dx = cx - focusX;
	float dy = cy - focusY;
	float dz = cz - focusZ;
	float maxR = range + radius;
	return (dx * dx + dy * dy + dz * dz) <= (maxR * maxR);
}

void Scene::Draw(
	DXRender* render,
	MeshRenderContext& ctx,
	const Frustum& frustum,
	const std::vector<SceneInstance>& instances,
	AlphaFilter alphaFilter,
	float shadowFocusX,
	float shadowFocusY,
	float shadowFocusZ,
	float shadowRange) const
{
	int filter = AlphaFilterToInt(alphaFilter);

	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		Model* model = inst.model;

		if (ctx.pass == MESH_PASS_SHADOW) {
			if (!InShadowRange(model, inst, shadowFocusX, shadowFocusY, shadowFocusZ, shadowRange))
				continue;
		} else if (!IsVisible(model, inst, frustum)) {
			continue;
		}

		model->SetPosition(
			inst.x, inst.y, inst.z,
			inst.scale[0], inst.scale[1], inst.scale[2],
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3]
		);
		model->Render(render, ctx, filter);
	}
}

void Scene::SortAlphaBackToFront(Camera* camera)
{
	XMVECTOR camPos = camera->GetPosition();
	float cx = XMVectorGetX(camPos);
	float cy = XMVectorGetY(camPos);
	float cz = XMVectorGetZ(camPos);

	std::sort(m_alpha.begin(), m_alpha.end(),
		[cx, cy, cz](const SceneInstance& a, const SceneInstance& b) {
			float dax = a.x - cx;
			float day = a.y - cy;
			float daz = a.z - cz;
			float dbx = b.x - cx;
			float dby = b.y - cy;
			float dbz = b.z - cz;
			float da = dax * dax + day * day + daz * daz;
			float db = dbx * dbx + dby * dby + dbz * dbz;
			return da > db;
		});
}
