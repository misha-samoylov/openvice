#pragma once

#include <vector>
#include <istream>

#include "../renderware.h"
#include "FrameList.h"
#include "LightList.h"
#include "AtomicList.h"
#include "Geometry.h"

using namespace std;

class Clump
{
public:
	void Read(char* bytes);
	void ReadExtension(char* bytes, size_t* offset);
	void Dump(bool detailed = false);
	void Clear();

	Geometry **GetGeometryList();
	FrameList* GetFrameList();
	AtomicList* GetAtomicList();
	LightList* GetLightList();

	uint32_t m_numGeometries;

private:
	AtomicList *m_atomicList;
	FrameList *m_frameList;
	LightList *m_lightList;
	Geometry **m_geometryList;

	/* Extensions */
	/* collision file */
	bool m_hasCollision;
	uint8_t *m_colData;
};