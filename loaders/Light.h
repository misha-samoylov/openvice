#pragma once

#include "../renderware.h"

class Light
{
private:
	float radius;
	float color[3];
	float minusCosAngle;
	uint32_t type;
	uint32_t flags;
public:
	int32_t frameIndex;
	void read(char* bytes, size_t* offset);
};
