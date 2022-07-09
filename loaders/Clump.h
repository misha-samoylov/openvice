#pragma once

#include <vector>
#include <istream>

#include "../renderware.h"

using namespace std;

class Clump
{
public:
	std::vector<Atomic> atomicList;
	std::vector<Frame> frameList;
	std::vector<Geometry> geometryList;
	std::vector<Light> lightList;

	/* Extensions */
	/* collision file */
	bool hasCollision;
	std::vector<uint8_t> colData;

	/* functions */
	void read(istream &dff);
	void readExtension(istream &dff);
	void dump(bool detailed = false);
	void clear(void);
};

