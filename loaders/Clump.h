#pragma once

#include <vector>
#include <istream>

#include "../renderware.h"

using namespace std;

class Clump
{
private:
	std::vector<Atomic> m_atomicList;
	std::vector<Frame> m_frameList;
	std::vector<Geometry> m_geometryList;
	std::vector<Light> m_lightList;

	/* Extensions */
	/* collision file */
	bool m_hasCollision;
	std::vector<uint8_t> m_colData;

public:
	void Read(istream &dff);
	void ReadExtension(istream &dff);
	void Dump(bool detailed = false);
	void Clear();

	std::vector<Geometry> GetGeometryList();
	std::vector<Light> GetLightList();
	std::vector<Frame> GetFrameList();
	std::vector<Atomic> GetAtomicList();
};