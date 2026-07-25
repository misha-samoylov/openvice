#include "COL.hpp"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

namespace {

std::string ToLowerKey(const char* name)
{
	std::string s;
	s.reserve(24);
	for (const char* p = name; *p; ++p)
		s.push_back((char)tolower((unsigned char)*p));
	return s;
}

} // namespace

bool COL::LoadAllFromIMG(IMG* img)
{
	if (!img)
		return false;

	int count = img->GetFileCount();
	int loadedFiles = 0;
	int loadedModels = 0;

	for (int i = 0; i < count; i++) {
		const char* fname = img->GetFilenameById((uint32_t)i);
		if (!fname)
			continue;

		size_t len = strlen(fname);
		if (len < 4)
			continue;
		if (_stricmp(fname + len - 4, ".col") != 0)
			continue;

		char* fileData = img->GetFileById((uint32_t)i);
		int32_t fileSize = img->GetFileSize((uint32_t)i);
		if (!fileData || fileSize <= 0)
			continue;

		size_t before = m_owned.size();
		if (LoadCollisionFile((const uint8_t*)fileData, fileSize)) {
			loadedFiles++;
			loadedModels += (int)(m_owned.size() - before);
		}
	}

	printf("[Info] COL: loaded %d archives, %d collision models\n", loadedFiles, loadedModels);
	return loadedModels > 0;
}

bool COL::LoadCollisionFile(const uint8_t* data, int32_t size)
{
	/* Same container loop as re3 CFileLoader::LoadCollisionFile. */
	int32_t remaining = size;
	const uint8_t* ptr = data;

	while (remaining > 8) {
		uint32_t ident = *(const uint32_t*)ptr;
		uint32_t modelsize = *(const uint32_t*)(ptr + 4);

		/* 'COLL' little-endian = 0x4C4C4F43 */
		if (ident != 0x4C4C4F43)
			return remaining < IMG_BLOCK_SIZE;

		if ((int32_t)(8 + modelsize) > remaining)
			break;

		char modelname[24];
		memcpy(modelname, ptr + 8, 24);
		modelname[23] = '\0';

		ColModel* model = new ColModel();
		memset(model->name, 0, sizeof(model->name));
		strncpy(model->name, modelname, 23);

		const uint8_t* body = ptr + 32; /* skip header + 24-byte name */
		uint32_t bodySize = modelsize - 24;
		if (!LoadCollisionModel(body, bodySize, *model)) {
			delete model;
		} else {
			model->CalculatePlanes();
			std::string key = ToLowerKey(model->name);
			if (m_models.find(key) == m_models.end()) {
				m_models[key] = model;
				m_owned.push_back(model);
			} else {
				/* Prefer first definition; free duplicate. */
				delete model;
			}
		}

		ptr += 8 + modelsize;
		remaining -= (int32_t)(8 + modelsize);
	}
	return true;
}

bool COL::LoadCollisionModel(const uint8_t* buf, uint32_t dataSize, ColModel& model)
{
	/* COL1 layout matching re3 CFileLoader::LoadCollisionModel. */
	if (dataSize < 44)
		return false;

	const uint8_t* end = buf + dataSize;

	float radius = *(const float*)(buf);
	float cx = *(const float*)(buf + 4);
	float cy = *(const float*)(buf + 8);
	float cz = *(const float*)(buf + 12);
	float minx = *(const float*)(buf + 16);
	float miny = *(const float*)(buf + 20);
	float minz = *(const float*)(buf + 24);
	float maxx = *(const float*)(buf + 28);
	float maxy = *(const float*)(buf + 32);
	float maxz = *(const float*)(buf + 36);
	int16_t numSpheres = *(const int16_t*)(buf + 40);
	buf += 44;

	model.boundSphere.center = GtaToEngineVec(cx, cy, cz);
	model.boundSphere.radius = radius;
	model.boundSphere.surface = 0;
	model.boundSphere.piece = 0;

	/* AABB after Y/Z swap. */
	model.boundBox.min = ColVec3(minx, minz, miny);
	model.boundBox.max = ColVec3(maxx, maxz, maxy);
	if (model.boundBox.min.y > model.boundBox.max.y) {
		float t = model.boundBox.min.y; model.boundBox.min.y = model.boundBox.max.y; model.boundBox.max.y = t;
	}
	if (model.boundBox.min.z > model.boundBox.max.z) {
		float t = model.boundBox.min.z; model.boundBox.min.z = model.boundBox.max.z; model.boundBox.max.z = t;
	}
	model.boundBox.surface = 0;
	model.boundBox.piece = 0;

	if (numSpheres < 0 || buf + numSpheres * 20 > end)
		return false;

	model.spheres.resize((size_t)numSpheres);
	for (int i = 0; i < numSpheres; i++) {
		float r = *(const float*)buf;
		float sx = *(const float*)(buf + 4);
		float sy = *(const float*)(buf + 8);
		float sz = *(const float*)(buf + 12);
		model.spheres[i].center = GtaToEngineVec(sx, sy, sz);
		model.spheres[i].radius = r;
		model.spheres[i].surface = buf[16];
		model.spheres[i].piece = buf[17];
		buf += 20;
	}

	if (buf + 4 > end)
		return false;
	int16_t numLines = *(const int16_t*)buf;
	buf += 4;
	if (numLines < 0 || buf + numLines * 24 > end)
		return false;
	/* Skip unused lines (re3 also discards them). */
	buf += numLines * 24;

	if (buf + 4 > end)
		return false;
	int16_t numBoxes = *(const int16_t*)buf;
	buf += 4;
	if (numBoxes < 0 || buf + numBoxes * 28 > end)
		return false;

	model.boxes.resize((size_t)numBoxes);
	for (int i = 0; i < numBoxes; i++) {
		float bminx = *(const float*)buf;
		float bminy = *(const float*)(buf + 4);
		float bminz = *(const float*)(buf + 8);
		float bmaxx = *(const float*)(buf + 12);
		float bmaxy = *(const float*)(buf + 16);
		float bmaxz = *(const float*)(buf + 20);
		ColVec3 mn = GtaToEngineVec(bminx, bminy, bminz);
		ColVec3 mx = GtaToEngineVec(bmaxx, bmaxy, bmaxz);
		model.boxes[i].min = ColVec3(
			mn.x < mx.x ? mn.x : mx.x,
			mn.y < mx.y ? mn.y : mx.y,
			mn.z < mx.z ? mn.z : mx.z);
		model.boxes[i].max = ColVec3(
			mn.x > mx.x ? mn.x : mx.x,
			mn.y > mx.y ? mn.y : mx.y,
			mn.z > mx.z ? mn.z : mx.z);
		model.boxes[i].surface = buf[24];
		model.boxes[i].piece = buf[25];
		buf += 28;
	}

	if (buf + 4 > end)
		return false;
	int16_t numVertices = *(const int16_t*)buf;
	buf += 4;
	if (numVertices < 0 || buf + numVertices * 12 > end)
		return false;

	model.vertices.resize((size_t)numVertices);
	for (int i = 0; i < numVertices; i++) {
		float vx = *(const float*)buf;
		float vy = *(const float*)(buf + 4);
		float vz = *(const float*)(buf + 8);
		model.vertices[i] = GtaToEngineVec(vx, vy, vz);
		buf += 12;
	}

	if (buf + 4 > end)
		return false;
	int16_t numTriangles = *(const int16_t*)buf;
	buf += 4;
	if (numTriangles < 0 || buf + numTriangles * 16 > end)
		return false;

	model.triangles.resize((size_t)numTriangles);
	for (int i = 0; i < numTriangles; i++) {
		int32_t a = *(const int32_t*)buf;
		int32_t b = *(const int32_t*)(buf + 4);
		int32_t c = *(const int32_t*)(buf + 8);
		if (a < 0 || b < 0 || c < 0 || a >= numVertices || b >= numVertices || c >= numVertices)
			return false;
		model.triangles[i].a = (uint16_t)a;
		model.triangles[i].b = (uint16_t)b;
		model.triangles[i].c = (uint16_t)c;
		model.triangles[i].surface = buf[12];
		buf += 16;
	}

	return true;
}

ColModel* COL::FindByName(const char* name)
{
	if (!name)
		return nullptr;
	std::unordered_map<std::string, ColModel*>::iterator it = m_models.find(ToLowerKey(name));
	if (it == m_models.end())
		return nullptr;
	return it->second;
}

void COL::Cleanup()
{
	for (size_t i = 0; i < m_owned.size(); i++)
		delete m_owned[i];
	m_owned.clear();
	m_models.clear();
}
