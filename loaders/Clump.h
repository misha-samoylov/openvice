#pragma once

#include <vector>
#include <istream>

#include "../renderware.h"
#include "FrameList.h"
#include "LightList.h"
#include "AtomicList.h"

using namespace std;

class Clump
{
private:
	AtomicList *m_atomicList;
	FrameList *m_frameList;
	LightList *m_lightList;
	std::vector<Geometry> m_geometryList;

	uint32_t m_numLights;
	uint32_t m_numAtomics;

	/* Extensions */
	/* collision file */
	bool m_hasCollision;
	std::vector<uint8_t> m_colData;

public:
	void Read(char *bytes);
	void ReadExtension(char *bytes, size_t *offset);
	void Dump(bool detailed = false);
	void Clear();

	std::vector<Geometry> GetGeometryList();
	FrameList* GetFrameList();
	AtomicList* GetAtomicList();
	LightList* GetLightList();
};