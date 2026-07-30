#pragma once

#include <vector>
#include "Model.h"
#include "assets/AssetRegistry.h"
#include "Frustum.h"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "core/AlphaFilter.h"

class CollisionWorld;

struct SceneInstance
{
	Model* model;
	float x, y, z;
	float scale[3];
	float rotation[4];
};

class Scene
{
public:
	void BuildFromAssets(AssetRegistry& assets, int worldHour);
	void SortAlphaBackToFront(Camera* camera);

	std::vector<SceneInstance>& Opaque() { return m_opaque; }
	std::vector<SceneInstance>& Alpha() { return m_alpha; }
	const std::vector<SceneInstance>& Opaque() const { return m_opaque; }
	const std::vector<SceneInstance>& Alpha() const { return m_alpha; }

	/*
	 * Draw after frustum + DRAW_DISTANCE + optional occlusion (Bullet rays).
	 * cam* / maxDist used for distance cull; collision may be null.
	 */
	int Draw(
		DXRender* render,
		MeshRenderContext& ctx,
		const Frustum& frustum,
		const std::vector<SceneInstance>& instances,
		AlphaFilter alphaFilter,
		float camX, float camY, float camZ,
		float maxDist,
		const CollisionWorld* collision = nullptr,
		float shadowFocusX = 0.0f,
		float shadowFocusY = 0.0f,
		float shadowFocusZ = 0.0f,
		float shadowRange = 0.0f
	) const;

	/* Average opaque instance origin — useful when only a sub-map (e.g. starisl) is loaded. */
	bool GetSpawnHint(float* outX, float* outY, float* outZ) const;

	/* Place a single already-loaded model at the origin (demo / isolation). */
	bool BuildSingleObject(AssetRegistry& assets, int modelId);

private:
	static bool IsVisible(
		Model* model, const SceneInstance& inst, const Frustum& frustum,
		float camX, float camY, float camZ, float maxDist,
		const CollisionWorld* collision);
	static bool IsOccluded(
		float camX, float camY, float camZ,
		float cx, float cy, float cz, float radius,
		const CollisionWorld* collision);
	static bool InShadowRange(
		Model* model, const SceneInstance& inst,
		float focusX, float focusY, float focusZ, float range);

	std::vector<SceneInstance> m_opaque;
	std::vector<SceneInstance> m_alpha;
};
