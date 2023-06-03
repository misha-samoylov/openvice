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
	void Read(char* bytes, size_t* offset);
};

class LightList
{
private:
	Light** m_lightList;
	uint32_t m_numLights;

public:
	void Read(uint32_t numLights, char* bytes, size_t* offset);
	void Cleanup();
	Light** GetLightList();
};