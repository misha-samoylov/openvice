#include "graphics/TextureFactory.h"
#include "DXRender.hpp"
#include "Mesh.hpp"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <DirectXTex.h>
#include <Dds.h>

using namespace DirectX;

namespace TextureFactory
{
	HRESULT CreateSrvFromDxt(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		uint32_t dxtCompression,
		ID3D11ShaderResourceView** outSrv
	)
	{
		if (!render || !data || !outSrv || size == 0)
			return E_INVALIDARG;

		*outSrv = nullptr;

		DDS_File dds;
		ZeroMemory(&dds, sizeof(dds));
		dds.dwMagic = DDS_MAGIC;
		dds.header.size = sizeof(DDS_HEADER);
		dds.header.width = width;
		dds.header.height = height;
		dds.header.pitchOrLinearSize = width * height;
		dds.header.ddspf.size = sizeof(DDS_PIXELFORMAT);
		dds.header.ddspf.flags = DDS_FOURCC;

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

		ScratchImage image;
		HRESULT hr = LoadFromDDSMemory(buf, len, DDS_FLAGS_NONE, nullptr, image);
		if (SUCCEEDED(hr)) {
			hr = CreateShaderResourceView(
				render->GetDevice(),
				image.GetImages(),
				image.GetImageCount(),
				image.GetMetadata(),
				outSrv
			);
		} else {
			printf("[Error] TextureFactory: LoadFromDDSMemory failed\n");
		}

		free(buf);
		return hr;
	}
}
