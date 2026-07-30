#include "world/Scene.h"
#include "assets/DffLoader.h"
#include "core/GameConfig.h"
#include "CollisionWorld.h"
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

bool Scene::GetSpawnHint(float* outX, float* outY, float* outZ) const
{
	if (!outX || !outY || !outZ || m_opaque.empty())
		return false;

	double sx = 0.0, sy = 0.0, sz = 0.0;
	const size_t n = m_opaque.size();
	for (size_t i = 0; i < n; i++) {
		sx += m_opaque[i].x;
		sy += m_opaque[i].y;
		sz += m_opaque[i].z;
	}
	*outX = (float)(sx / (double)n);
	*outY = (float)(sy / (double)n) + 5.0f;
	*outZ = (float)(sz / (double)n);
	return true;
}

bool Scene::BuildSingleObject(AssetRegistry& assets, int modelId)
{
	m_opaque.clear();
	m_alpha.clear();

	Model* model = assets.FindModelById(modelId);
	if (!model) {
		printf("[Error] BuildSingleObject: model id %d not loaded\n", modelId);
		return false;
	}

	SceneInstance inst = {};
	inst.model = model;
	inst.x = 0.0f;
	inst.y = 0.0f;
	inst.z = 0.0f;
	inst.scale[0] = inst.scale[1] = inst.scale[2] = 1.0f;
	inst.rotation[0] = 0.0f;
	inst.rotation[1] = 0.0f;
	inst.rotation[2] = 0.0f;
	inst.rotation[3] = 1.0f;

	if (model->IsAlpha())
		m_alpha.push_back(inst);
	else
		m_opaque.push_back(inst);

	printf("[Info] Single-object scene: id=%d name=%s opaque=%u alpha=%u\n",
		modelId, model->GetName().c_str(),
		(unsigned)m_opaque.size(), (unsigned)m_alpha.size());
	return true;
}

bool Scene::IsOccluded(
	float camX, float camY, float camZ,
	float cx, float cy, float cz, float radius,
	const CollisionWorld* collision)
{
	if (!collision)
		return false;

	float dx = cx - camX;
	float dy = cy - camY;
	float dz = cz - camZ;
	float distSq = dx * dx + dy * dy + dz * dz;
	if (distSq < 1.0f)
		return false;

	float dist = sqrtf(distSq);
	/* Huge / nearby volumes stay visible (conservative). */
	if (dist < 12.0f || radius > 60.0f)
		return false;

	float inv = 1.0f / dist;
	float dirX = dx * inv;
	float dirY = dy * inv;
	float dirZ = dz * inv;

	auto rayBlocked = [&](float tx, float ty, float tz) -> bool {
		float tdx = tx - camX;
		float tdy = ty - camY;
		float tdz = tz - camZ;
		float reach = sqrtf(tdx * tdx + tdy * tdy + tdz * tdz);
		if (reach < 1.0f)
			return false;

		float hx = 0.0f, hy = 0.0f, hz = 0.0f;
		if (!collision->RayTestClosest(camX, camY, camZ, tx, ty, tz, &hx, &hy, &hz, nullptr))
			return false;

		float hdx = hx - camX;
		float hdy = hy - camY;
		float hdz = hz - camZ;
		float hitDist = sqrtf(hdx * hdx + hdy * hdy + hdz * hdz);
		/* Hit clearly in front of the target sample → occluder. */
		return hitDist < (reach - 0.75f);
	};

	/* Aim just before the sphere surface (avoid self-hit on own COL). */
	float surface = dist - radius * 0.9f;
	if (surface < 2.0f)
		return false;

	float midX = camX + dirX * surface;
	float midY = camY + dirY * surface;
	float midZ = camZ + dirZ * surface;

	/* Two probes: center line + raised — keep if either is clear. */
	float upY = cy + radius * 0.65f;
	float upSurface = sqrtf(dx * dx + (upY - camY) * (upY - camY) + dz * dz) - radius * 0.5f;
	if (upSurface < 2.0f)
		upSurface = surface;
	float upInv = 1.0f / sqrtf(dx * dx + (upY - camY) * (upY - camY) + dz * dz);
	float upX = camX + dx * upInv * upSurface;
	float upTY = camY + (upY - camY) * upInv * upSurface;
	float upZ = camZ + dz * upInv * upSurface;

	if (!rayBlocked(midX, midY, midZ))
		return false;
	if (!rayBlocked(upX, upTY, upZ))
		return false;
	return true;
}

bool Scene::IsVisible(
	Model* model, const SceneInstance& inst, const Frustum& frustum,
	float camX, float camY, float camZ, float maxDist,
	const CollisionWorld* collision)
{
	float cx, cy, cz, radius;
	model->GetWorldCullSphere(
		inst.x, inst.y, inst.z,
		inst.scale[0], inst.scale[1], inst.scale[2],
		&cx, &cy, &cz, &radius
	);

	float dx = cx - camX;
	float dy = cy - camY;
	float dz = cz - camZ;
	float distSq = dx * dx + dy * dy + dz * dz;
	float maxR = maxDist + radius;
	if (distSq > maxR * maxR)
		return false;

	if (!frustum.CheckSphere(cx, cy, cz, radius))
		return false;

#if ENABLE_OCCLUSION_CULLING
	if (IsOccluded(camX, camY, camZ, cx, cy, cz, radius, collision))
		return false;
#else
	(void)collision;
#endif
	return true;
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

int Scene::Draw(
	DXRender* render,
	MeshRenderContext& ctx,
	const Frustum& frustum,
	const std::vector<SceneInstance>& instances,
	AlphaFilter alphaFilter,
	float camX, float camY, float camZ,
	float maxDist,
	const CollisionWorld* collision,
	float shadowFocusX,
	float shadowFocusY,
	float shadowFocusZ,
	float shadowRange) const
{
	int filter = AlphaFilterToInt(alphaFilter);
	int draws = 0;

	for (size_t i = 0; i < instances.size(); i++) {
		const SceneInstance& inst = instances[i];
		Model* model = inst.model;

		if (ctx.pass == MESH_PASS_SHADOW) {
			if (!InShadowRange(model, inst, shadowFocusX, shadowFocusY, shadowFocusZ, shadowRange))
				continue;
		} else if (!IsVisible(model, inst, frustum, camX, camY, camZ, maxDist, collision)) {
			continue;
		}

		model->SetPosition(
			inst.x, inst.y, inst.z,
			inst.scale[0], inst.scale[1], inst.scale[2],
			inst.rotation[0], inst.rotation[1], inst.rotation[2], inst.rotation[3]
		);
		model->Render(render, ctx, filter);
		draws++;
	}
	return draws;
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
