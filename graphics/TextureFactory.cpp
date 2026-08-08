#include "graphics/TextureFactory.h"
#include "DXRender.hpp"
#include "Mesh.hpp"
#include "renderware.h"

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

	HRESULT CreateSrvFromRgba(
		DXRender* render,
		const uint8_t* data,
		size_t size,
		uint32_t width,
		uint32_t height,
		ID3D11ShaderResourceView** outSrv
	)
	{
		if (!render || !data || !outSrv || width == 0 || height == 0)
			return E_INVALIDARG;
		*outSrv = nullptr;

		const size_t expected = (size_t)width * (size_t)height * 4u;
		if (size < expected)
			return E_INVALIDARG;

		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA init;
		ZeroMemory(&init, sizeof(init));
		init.pSysMem = data;
		init.SysMemPitch = width * 4;

		ID3D11Texture2D* tex = nullptr;
		HRESULT hr = render->GetDevice()->CreateTexture2D(&desc, &init, &tex);
		if (FAILED(hr))
			return hr;

		hr = render->GetDevice()->CreateShaderResourceView(tex, nullptr, outSrv);
		tex->Release();
		return hr;
	}

	HRESULT CreateSrvFromNative(
		DXRender* render,
		NativeTexture& tex,
		ID3D11ShaderResourceView** outSrv
	)
	{
		if (!outSrv)
			return E_INVALIDARG;
		*outSrv = nullptr;
		if (tex.texels.empty() || !tex.texels[0] || tex.width.empty() || tex.height.empty())
			return E_INVALIDARG;

		if (tex.dxtCompression == 0 && (tex.rasterFormat & (RASTER_PAL8 | RASTER_PAL4)))
			tex.convertTo32Bit();

		if (tex.dxtCompression != 0) {
			return CreateSrvFromDxt(
				render,
				tex.texels[0],
				tex.dataSizes[0],
				tex.width[0],
				tex.height[0],
				tex.dxtCompression,
				outSrv
			);
		}

		if (tex.depth != 0x20 && tex.depth != 32)
			tex.convertTo32Bit();

		return CreateSrvFromRgba(
			render,
			tex.texels[0],
			tex.dataSizes[0],
			tex.width[0],
			tex.height[0],
			outSrv
		);
	}
}
