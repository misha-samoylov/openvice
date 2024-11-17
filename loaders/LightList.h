#pragma once

#include "../renderware.h"

class Light
{
public:
	void Read(char* bytes, size_t* offset);
	void SetFrameIndex(int32_t frameIndex);

private:
	int32_t m_frameIndex;
	float m_radius;
	float m_color[3];
	float m_minusCosAngle;
	uint32_t m_type;
	uint32_t m_flags;
};

class LightList
{
public:
	void Read(uint32_t numLights, char* bytes, size_t* offset);
	void Cleanup();
	Light** GetLightList();

private:
	Light** m_lightList;
	uint32_t m_numLights;
};