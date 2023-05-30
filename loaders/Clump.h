#pragma once

#include <vector>
#include <istream>

#include "../renderware.h"
#include "FrameList.h"

using namespace std;

class Clump
{
private:
	std::vector<Atomic> m_atomicList;
	FrameList **m_frameList;
	std::vector<Geometry> m_geometryList;
	std::vector<Light> m_lightList;

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
	std::vector<Light> GetLightList();
	FrameList **GetFrameList();
	std::vector<Atomic> GetAtomicList();
};