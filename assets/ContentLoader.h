#pragma once

#include "assets/AssetRegistry.h"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"

class ContentLoader
{
public:
	/* Load IDE/IPL/TXD/DFF map content into assets. */
	bool LoadMapContent(IMG* img, DXRender* render, AssetRegistry& assets);

	/* Load TXD archive textures referenced by name (no extension). */
	static void LoadTexturesFromTxd(IMG* img, AssetRegistry& assets, const char* archiveName);

	static const char* const* MapNames();
	static int MapCount();
};
