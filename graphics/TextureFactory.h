#pragma once

#include <stdint.h>
#include <d3d11.h>

class DXRender;

namespace TextureFactory
{
	/* Build an SRV from raw DXT texel payload (no DDS header). */
	HRESULT CreateSrvFromDxt(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		uint32_t dxtCompression,
		ID3D11ShaderResourceView** outSrv
	);
}
