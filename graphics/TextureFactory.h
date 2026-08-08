#pragma once

#include <stdint.h>
#include <d3d11.h>

class DXRender;

struct NativeTexture;

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

	/* Uncompressed BGRA8 / RGBA8 (after NativeTexture::convertTo32Bit). */
	HRESULT CreateSrvFromRgba(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		ID3D11ShaderResourceView** outSrv
	);

	/* DXT or uncompressed mip0 from a parsed NativeTexture. */
	HRESULT CreateSrvFromNative(
		DXRender* render,
		NativeTexture& tex,
		ID3D11ShaderResourceView** outSrv
	);
}
