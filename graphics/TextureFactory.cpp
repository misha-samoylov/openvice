#include "graphics/TextureFactory.h"
#include "Mesh.hpp"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#include <DirectXTex.h>
#include <Dds.h>

using namespace DirectX;

namespace TextureFactory
{
	static HRESULT CreateUncompressed(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		GpuTexture* outTex)
	{
		const size_t rgbaBytes = (size_t)width * (size_t)height * 4u;
		std::vector<uint8_t> rgba;

		if (size >= rgbaBytes) {
			/* Already 32 bpp (after convertTo32Bit) — RGBA. */
			return render->CreateGpuTextureFromPixels(
				data, width, height, width * 4, DXGI_FORMAT_R8G8B8A8_UNORM, outTex);
		}

		if (size >= (size_t)width * height * 2u) {
			/* 16 bpp A1R5G5B5 → RGBA8 */
			rgba.resize(rgbaBytes);
			const uint16_t* src = (const uint16_t*)data;
			for (uint32_t i = 0; i < width * height; i++) {
				uint16_t c = src[i];
				rgba[i * 4 + 0] = (uint8_t)(((c >> 10) & 0x1F) * 255 / 31);
				rgba[i * 4 + 1] = (uint8_t)(((c >> 5) & 0x1F) * 255 / 31);
				rgba[i * 4 + 2] = (uint8_t)(((c >> 0) & 0x1F) * 255 / 31);
				rgba[i * 4 + 3] = (c & 0x8000) ? 255 : 0;
			}
			return render->CreateGpuTextureFromPixels(
				rgba.data(), width, height, width * 4, DXGI_FORMAT_R8G8B8A8_UNORM, outTex);
		}

		printf("[Error] TextureFactory: unsupported uncompressed size %zu for %ux%u\n",
			size, width, height);
		return E_FAIL;
	}

	HRESULT CreateSrvFromDxt(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		uint32_t dxtCompression,
		GpuTexture* outTex
	)
	{
		if (!render || !data || !outTex || size == 0 || width == 0 || height == 0)
			return E_INVALIDARG;

		outTex->resource = nullptr;
		outTex->srvIndex = UINT_MAX;

		/* Uncompressed / expanded textures — never wrap as fake DXT. */
		if (dxtCompression == 0)
			return CreateUncompressed(render, data, size, width, height, outTex);

		const uint32_t blocksW = (std::max)(1u, (width + 3u) / 4u);
		const uint32_t blocksH = (std::max)(1u, (height + 3u) / 4u);
		const uint32_t bytesPerBlock = (dxtCompression <= 1) ? 8u : 16u;

		DDS_File dds;
		ZeroMemory(&dds, sizeof(dds));
		dds.dwMagic = DDS_MAGIC;
		dds.header.size = sizeof(DDS_HEADER);
		dds.header.flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_LINEARSIZE;
		dds.header.width = width;
		dds.header.height = height;
		dds.header.pitchOrLinearSize = blocksW * blocksH * bytesPerBlock;
		dds.header.ddspf.size = sizeof(DDS_PIXELFORMAT);
		dds.header.ddspf.flags = DDS_FOURCC;
		dds.header.caps = DDS_SURFACE_FLAGS_TEXTURE;

		switch (dxtCompression) {
		case 3:
			dds.header.ddspf.fourCC = FOURCC_DXT3;
			break;
		case 4:
			dds.header.ddspf.fourCC = FOURCC_DXT4;
			break;
		case 5:
			dds.header.ddspf.fourCC = FOURCC_DXT5;
			break;
		default:
			dds.header.ddspf.fourCC = FOURCC_DXT1;
			break;
		}

		size_t len = sizeof(dds) + size;
		uint8_t* buf = (uint8_t*)malloc(len);
		if (!buf)
			return E_OUTOFMEMORY;

		memcpy(buf, &dds, sizeof(dds));
		memcpy(buf + sizeof(dds), data, size);

		HRESULT hr = render->CreateGpuTextureFromDdsMemory(buf, len, outTex);
		if (FAILED(hr)) {
			printf("[Error] TextureFactory: CreateGpuTextureFromDdsMemory failed (0x%08X, %ux%u dxt%u)\n",
				(unsigned)hr, width, height, dxtCompression);
		}

		free(buf);
		return hr;
	}
}
