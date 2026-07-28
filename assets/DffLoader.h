#pragma once

#include "assets/AssetRegistry.h"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"

namespace DffLoader
{
	/* Map / world DFFs — skip LOD atomics, remap GTA→engine in vertices. */
	int LoadMapDff(IMG* img, DXRender* render, AssetRegistry& assets, char* name, int modelId);

	/* Vehicle DFFs — bake frame LTM, filter damaged/VLO/extras. */
	int LoadVehicleDff(IMG* img, DXRender* render, AssetRegistry& assets, char* name, int modelId);

	bool IsLodModelName(const char* name);
	bool IsNightModelName(const char* name);
	bool IsDayModelName(const char* name);
	void EnsureDayNightModelTimes(AssetRegistry& assets);
}
