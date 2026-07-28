#pragma once

#include <stdint.h>
#include <vector>
#include <string.h>

#include "renderware.h" /* MAX_TEXTURE_NAME */

struct GameMaterial
{
	char name[MAX_TEXTURE_NAME];
	std::vector<uint8_t> source;
	uint32_t width;
	uint32_t height;
	uint32_t dxtCompression;
	uint32_t depth;
	bool isAlpha;

	GameMaterial()
		: width(0)
		, height(0)
		, dxtCompression(0)
		, depth(0)
		, isAlpha(false)
	{
		memset(name, 0, sizeof(name));
	}

	int Size() const { return (int)source.size(); }
	uint8_t* Data() { return source.empty() ? nullptr : source.data(); }
	const uint8_t* Data() const { return source.empty() ? nullptr : source.data(); }
};
