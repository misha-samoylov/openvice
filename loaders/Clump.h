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

	uint32_t m_numGeometries = 0;

private:
	AtomicList *m_atomicList = nullptr;
	FrameList *m_frameList = nullptr;
	LightList *m_lightList = nullptr;
	Geometry **m_geometryList = nullptr;

	/* Extensions */
	/* collision file */
	bool m_hasCollision = false;
	uint8_t *m_colData = nullptr;
};