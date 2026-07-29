#pragma once

#include <stdint.h>

#include "DXRender.hpp"

namespace TextureFactory
{
	/* Build a GpuTexture from raw DXT texel payload (no DDS header). */
	HRESULT CreateSrvFromDxt(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		uint32_t dxtCompression,
		GpuTexture* outTex
	);
}
