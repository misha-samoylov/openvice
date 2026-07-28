#pragma once

/* Which submeshes to draw from a model / scene instance. */
enum class AlphaFilter
{
	OpaqueOnly = -1, /* opaque meshes only */
	All = 0,         /* every mesh */
	Cutout = 1,      /* alpha-test / cutout */
	Soft = 2         /* soft translucent */
};

inline int AlphaFilterToInt(AlphaFilter f)
{
	return static_cast<int>(f);
}
