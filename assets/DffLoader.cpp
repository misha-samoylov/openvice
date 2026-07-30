#include "assets/DffLoader.h"
#include "core/GtaCoords.h"
#include "graphics/GpuTextureCache.h"
#include "loaders/Clump.h"
#include "loaders/Geometry.h"
#include "Mesh.hpp"

#include <float.h>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace DffLoader
{
	bool IsLodModelName(const char* name)
	{
		if (!name)
			return false;
		while (*name == ' ' || *name == '\t')
			name++;
		if (!name[0])
			return false;
		if (_strnicmp(name, "lod", 3) == 0)
			return true;
		if (_strnicmp(name, "IslandLOD", 9) == 0)
			return true;
		return false;
	}

	static const char* FindDayNightSuffix(const char* name)
	{
		if (!name)
			return nullptr;
		const char* p = strstr(name, "_nt");
		if (p && (p[3] == '\0' || p[3] == '.'))
			return p;
		p = strstr(name, "_dy");
		if (p && (p[3] == '\0' || p[3] == '.'))
			return p;
		return nullptr;
	}

	bool IsNightModelName(const char* name)
	{
		const char* p = FindDayNightSuffix(name);
		return p && p[1] == 'n';
	}

	bool IsDayModelName(const char* name)
	{
		const char* p = FindDayNightSuffix(name);
		return p && p[1] == 'd';
	}

	void EnsureDayNightModelTimes(AssetRegistry& assets)
	{
		int assigned = 0;
		for (size_t i = 0; i < assets.Models().size(); i++) {
			Model* m = assets.Models()[i].get();
			if (m->IsTimed())
				continue;
			const char* name = m->GetName().c_str();
			if (IsNightModelName(name)) {
				m->SetTimed(true, 21, 5);
				assigned++;
			} else if (IsDayModelName(name)) {
				m->SetTimed(true, 5, 21);
				assigned++;
			}
		}
		if (assigned > 0)
			printf("[Info] Assigned day/night hours to %d untimed _dy/_nt models\n", assigned);
	}

	static bool ShouldSkipMapAtomic(const char* name)
	{
		if (!name || !name[0])
			return true;
		if (strstr(name, "_dam") != NULL)
			return true;
		if (strstr(name, "_vlo") != NULL)
			return true;

		for (const char* p = name; *p; p++) {
			if (p[0] != '_')
				continue;
			if ((p[1] != 'L' && p[1] != 'l') || p[2] < '0' || p[2] > '9')
				continue;
			int lod = 0;
			for (const char* d = p + 2; *d >= '0' && *d <= '9'; d++)
				lod = lod * 10 + (*d - '0');
			if (lod != 0)
				return true;
		}
		return false;
	}

	static bool ShouldSkipVehicleAtomic(const char* name)
	{
		if (!name || !name[0])
			return true;
		if (strstr(name, "_dam") != NULL)
			return true;
		if (strstr(name, "_vlo") != NULL)
			return true;
		if (strstr(name, "_lo") != NULL)
			return true;
		if (_strnicmp(name, "extra", 5) == 0)
			return true;
		return false;
	}

	static XMMATRIX FrameWorldGtaMat(FrameList* frames, int idx)
	{
		if (idx < 0 || idx >= frames->GetNumFrames())
			return XMMatrixIdentity();
		Frame* f = frames->GetFrame(idx);
		XMMATRIX local = GtaCoords::FrameLocalMatrix(f->GetRotationMatrix(), f->GetPosition());
		int parent = f->GetParent();
		if (parent < 0)
			return local;
		return XMMatrixMultiply(local, FrameWorldGtaMat(frames, parent));
	}

	static bool HasI(const char* hay, const char* needle)
	{
		if (!hay || !needle || !needle[0])
			return false;
		size_t nlen = strlen(needle);
		for (const char* p = hay; *p; p++) {
			size_t i = 0;
			while (i < nlen) {
				char a = p[i];
				char b = needle[i];
				if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
				if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
				if (a != b)
					break;
				i++;
			}
			if (i == nlen)
				return true;
		}
		return false;
	}

	/* Palms / trees / bushes — cutout fronds get vertex wind, trunks stay still. */
	static bool IsFoliageWindModel(const char* name)
	{
		if (!name || !name[0])
			return false;
		if (_strnicmp(name, "veg_", 4) == 0)
			return true;
		if (HasI(name, "palm") || HasI(name, "bush") || HasI(name, "plant"))
			return true;
		if (HasI(name, "fern") || HasI(name, "grass") || HasI(name, "weed"))
			return true;
		/* "tree" but not "street" */
		if (_strnicmp(name, "tree", 4) == 0)
			return true;
		if (HasI(name, "_tree") || HasI(name, "trees"))
			return true;
		return false;
	}

	static void ApplyTexture(
		DXRender* render, Mesh* mesh, Model* model,
		AssetRegistry& assets, const char* matName)
	{
		GameMaterial* tex = nullptr;
		if (matName && matName[0]) {
			tex = assets.Txd().FindTexture(matName);
			if (!tex)
				tex = assets.Txd().FindTextureAnywhere(matName);
		}

		if (!tex) {
			/* Missing texture → black stub (always draw, never skip mesh). */
			GpuTexture black = GpuTextureCache::Instance().ResolveOrBlack(render, nullptr);
			mesh->SetSharedTexture(black);
			return;
		}

		bool isAlpha = tex->isAlpha;
		uint32_t dxt = tex->dxtCompression;
		mesh->SetAlpha(isAlpha);
		mesh->SetAlphaCutout(isAlpha && dxt != 3 && dxt != 4 && dxt != 5);
		if (!model->IsAlpha() && isAlpha)
			model->SetAlpha(true);
		/* Alpha fronds sway; opaque trunks stay put. */
		if (isAlpha && IsFoliageWindModel(model->GetName().c_str()))
			mesh->SetWindAmount(0.32f);

		GpuTexture gpu = GpuTextureCache::Instance().ResolveOrBlack(render, tex);
		mesh->SetSharedTexture(gpu);
	}

	static void ExpandBoundsFromVertices(
		Model* model, const float* meshVertexData, int vCount)
	{
		float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
		float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
		for (int v = 0; v < vCount; v++) {
			float vx = meshVertexData[v * 5 + 0];
			float vy = meshVertexData[v * 5 + 1];
			float vz = meshVertexData[v * 5 + 2];
			if (vx < minX) minX = vx;
			if (vy < minY) minY = vy;
			if (vz < minZ) minZ = vz;
			if (vx > maxX) maxX = vx;
			if (vy > maxY) maxY = vy;
			if (vz > maxZ) maxZ = vz;
		}
		float cx = (minX + maxX) * 0.5f;
		float cy = (minY + maxY) * 0.5f;
		float cz = (minZ + maxZ) * 0.5f;
		float hx = (maxX - minX) * 0.5f;
		float hy = (maxY - minY) * 0.5f;
		float hz = (maxZ - minZ) * 0.5f;
		float radius = sqrtf(hx * hx + hy * hy + hz * hz);
		model->IncludeBoundingSphere(cx, cy, cz, radius);
	}

	static void BuildMeshesFromGeometry(
		DXRender* render, AssetRegistry& assets, Model* model,
		Geometry* geometry, const float* meshVertexData, int vCount)
	{
		for (uint32_t si = 0; si < geometry->splits.size(); si++) {
			std::vector<uint32_t> triIndices;
			geometry->CollectSplitTriangles(si, triIndices);
			if (triIndices.empty())
				continue;

			Mesh* mesh = new Mesh();
			mesh->Init(
				render,
				const_cast<float*>(meshVertexData),
				vCount * 5,
				triIndices.data(),
				(int)triIndices.size(),
				D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
			);

			uint32_t materialIndex = geometry->splits[si].matIndex;
			if (materialIndex < geometry->m_numMaterials) {
				Material* material = geometry->materialList[materialIndex];
				ApplyTexture(render, mesh, model, assets, material->texture.name);
			}
			model->AddMesh(mesh);
		}
	}

	int LoadMapDff(IMG* img, DXRender* render, AssetRegistry& assets, char* name, int modelId)
	{
		if (IsLodModelName(name))
			return 0;

		char result_name[MAX_LENGTH_FILENAME + 4];
		strcpy(result_name, name);
		strcat(result_name, ".dff");

		int fileId = img->GetFileIndexByName(result_name);
		if (fileId == -1)
			return 1;

		char* fileBuffer = (char*)img->GetFileById(fileId);
		Clump* clump = new Clump();
		clump->Read(fileBuffer);

		std::unique_ptr<Model> model(new Model());
		model->SetId(modelId);
		model->SetName(name);
		model->SetAlpha(false);

		std::vector<char> loadGeom(clump->m_numGeometries, 0);
		bool haveAtomicFilter = false;
		FrameList* frames = clump->GetFrameList();
		AtomicList* atomics = clump->GetAtomicList();
		if (frames && atomics && atomics->GetNumAtomic() > 0) {
			for (uint32_t ai = 0; ai < atomics->GetNumAtomic(); ai++) {
				Atomic* atomic = atomics->GetAtomic((int)ai);
				int frameIndex = atomic->GetFrameIndex();
				int geomIndex = atomic->GetGeometryIndex();
				if (frameIndex < 0 || frameIndex >= frames->GetNumFrames())
					continue;
				if (geomIndex < 0 || (uint32_t)geomIndex >= clump->m_numGeometries)
					continue;
				const char* frameName = frames->GetFrame(frameIndex)->GetName();
				if (ShouldSkipMapAtomic(frameName))
					continue;
				loadGeom[(size_t)geomIndex] = 1;
				haveAtomicFilter = true;
			}
		}

		for (uint32_t index = 0; index < clump->m_numGeometries; index++) {
			if (haveAtomicFilter && !loadGeom[index])
				continue;

			Geometry* geometry = clump->GetGeometryList()[index];

			{
				float* bs = geometry->boundingSphere;
				float ex, ey, ez;
				GtaCoords::ToEngine(bs[0], bs[1], bs[2], &ex, &ey, &ez);
				model->IncludeBoundingSphere(ex, ey, ez, bs[3]);

				if (geometry->vertices != NULL && geometry->vertexCount > 0) {
					float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
					float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
					for (uint32_t v = 0; v < geometry->vertexCount; v++) {
						float gx = geometry->vertices[v * 3 + 0];
						float gy = geometry->vertices[v * 3 + 1];
						float gz = geometry->vertices[v * 3 + 2];
						float vx, vy, vz;
						GtaCoords::ToEngine(gx, gy, gz, &vx, &vy, &vz);
						if (vx < minX) minX = vx;
						if (vy < minY) minY = vy;
						if (vz < minZ) minZ = vz;
						if (vx > maxX) maxX = vx;
						if (vy > maxY) maxY = vy;
						if (vz > maxZ) maxZ = vz;
					}
					float cx = (minX + maxX) * 0.5f;
					float cy = (minY + maxY) * 0.5f;
					float cz = (minZ + maxZ) * 0.5f;
					float hx = (maxX - minX) * 0.5f;
					float hy = (maxY - minY) * 0.5f;
					float hz = (maxZ - minZ) * 0.5f;
					float radius = sqrtf(hx * hx + hy * hy + hz * hz);
					model->IncludeBoundingSphere(cx, cy, cz, radius);
				}
			}

			int v_count = (int)geometry->vertexCount;
			std::vector<float> meshVertexData((size_t)v_count * 5);

			for (int v = 0; v < v_count; v++) {
				float gx = geometry->vertices[v * 3 + 0];
				float gy = geometry->vertices[v * 3 + 1];
				float gz = geometry->vertices[v * 3 + 2];
				float ex, ey, ez;
				GtaCoords::ToEngine(gx, gy, gz, &ex, &ey, &ez);

				float tx = 0.0f, ty = 0.0f;
				if (geometry->flags & FLAGS_TEXTURED) {
					tx = geometry->texCoords[0][v * 2 + 0];
					ty = geometry->texCoords[0][v * 2 + 1];
				}

				meshVertexData[(size_t)v * 5 + 0] = ex;
				meshVertexData[(size_t)v * 5 + 1] = ey;
				meshVertexData[(size_t)v * 5 + 2] = ez;
				meshVertexData[(size_t)v * 5 + 3] = tx;
				meshVertexData[(size_t)v * 5 + 4] = ty;
			}

			for (uint32_t i = 0; i < geometry->splits.size(); i++) {
				std::vector<uint32_t> triIndices;
				geometry->CollectSplitTriangles(i, triIndices);
				if (triIndices.empty())
					continue;

				Mesh* mesh = new Mesh();
				mesh->Init(
					render,
					meshVertexData.data(),
					v_count * 5,
					triIndices.data(),
					(int)triIndices.size(),
					D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
				);

				uint32_t materialIndex = geometry->splits[i].matIndex;
				if (materialIndex < geometry->m_numMaterials) {
					Material* material = geometry->materialList[materialIndex];
					ApplyTexture(render, mesh, model.get(), assets, material->texture.name);
				}
				model->AddMesh(mesh);
			}
		}

		clump->Clear();
		delete clump;

		assets.AddModel(std::move(model));
		return 0;
	}

	int LoadVehicleDff(IMG* img, DXRender* render, AssetRegistry& assets, char* name, int modelId)
	{
		char result_name[MAX_LENGTH_FILENAME + 4];
		strcpy(result_name, name);
		strcat(result_name, ".dff");

		int fileId = img->GetFileIndexByName(result_name);
		if (fileId == -1)
			return 1;

		char* fileBuffer = (char*)img->GetFileById(fileId);
		Clump* clump = new Clump();
		clump->Read(fileBuffer);

		FrameList* frames = clump->GetFrameList();
		AtomicList* atomics = clump->GetAtomicList();
		Geometry** geometries = clump->GetGeometryList();
		if (!frames || !atomics || !geometries) {
			clump->Clear();
			delete clump;
			return 1;
		}

		std::unique_ptr<Model> model(new Model());
		model->SetId(modelId);
		model->SetName(name);
		model->SetAlpha(false);

		int loadedAtomics = 0;
		for (uint32_t ai = 0; ai < atomics->GetNumAtomic(); ai++) {
			Atomic* atomic = atomics->GetAtomic((int)ai);
			int frameIndex = atomic->GetFrameIndex();
			int geomIndex = atomic->GetGeometryIndex();
			if (frameIndex < 0 || frameIndex >= frames->GetNumFrames())
				continue;
			if (geomIndex < 0 || (uint32_t)geomIndex >= clump->m_numGeometries)
				continue;

			Frame* frame = frames->GetFrame(frameIndex);
			const char* frameName = frame->GetName();
			if (ShouldSkipVehicleAtomic(frameName))
				continue;

			Geometry* geometry = geometries[geomIndex];
			if (!geometry || !geometry->vertices || geometry->vertexCount == 0)
				continue;

			XMMATRIX worldGta = FrameWorldGtaMat(frames, frameIndex);
			int v_count = (int)geometry->vertexCount;
			std::vector<float> meshVertexData((size_t)v_count * 5);

			for (int v = 0; v < v_count; v++) {
				float gx = geometry->vertices[v * 3 + 0];
				float gy = geometry->vertices[v * 3 + 1];
				float gz = geometry->vertices[v * 3 + 2];

				XMVECTOR local = XMVectorSet(gx, gy, gz, 1.0f);
				XMVECTOR world = XMVector3Transform(local, worldGta);
				float wx = XMVectorGetX(world);
				float wy = XMVectorGetY(world);
				float wz = XMVectorGetZ(world);

				float ex, ey, ez;
				GtaCoords::ToEngine(wx, wy, wz, &ex, &ey, &ez);

				float tx = 0.0f, ty = 0.0f;
				if (geometry->flags & FLAGS_TEXTURED) {
					tx = geometry->texCoords[0][v * 2 + 0];
					ty = geometry->texCoords[0][v * 2 + 1];
				}

				meshVertexData[(size_t)v * 5 + 0] = ex;
				meshVertexData[(size_t)v * 5 + 1] = ey;
				meshVertexData[(size_t)v * 5 + 2] = ez;
				meshVertexData[(size_t)v * 5 + 3] = tx;
				meshVertexData[(size_t)v * 5 + 4] = ty;
			}

			ExpandBoundsFromVertices(model.get(), meshVertexData.data(), v_count);
			BuildMeshesFromGeometry(render, assets, model.get(), geometry,
				meshVertexData.data(), v_count);
			loadedAtomics++;
		}

		clump->Clear();
		delete clump;

		if (loadedAtomics == 0) {
			printf("[Error] Vehicle DFF '%s': no atomics loaded\n", name);
			return 1;
		}

		printf("[Info] Vehicle '%s': %d atomics, %d meshes\n",
			name, loadedAtomics, (int)model->GetMeshes().size());
		assets.AddModel(std::move(model));
		return 0;
	}
}
