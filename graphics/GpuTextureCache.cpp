#include "graphics/GpuTextureCache.h"
#include "graphics/TextureFactory.h"

#include <stdio.h>
#include <string.h>

GpuTextureCache& GpuTextureCache::Instance()
{
	static GpuTextureCache s;
	return s;
}

std::string GpuTextureCache::MakeKey(const GameMaterial* mat)
{
	if (!mat)
		return std::string();
	char buf[160];
	sprintf(buf, "%s|%u|%u|%u|%u|%zu",
		mat->name,
		mat->width,
		mat->height,
		mat->dxtCompression,
		mat->depth,
		mat->source.size());
	return std::string(buf);
}

HRESULT GpuTextureCache::EnsureBlack(DXRender* render)
{
	if (!render)
		return E_INVALIDARG;
	if (m_black.Valid())
		return S_OK;

	const uint8_t pixels[4] = { 0, 0, 0, 255 };
	HRESULT hr = render->CreateGpuTextureFromPixels(
		pixels, 1, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, &m_black);
	if (FAILED(hr)) {
		printf("[Error] GpuTextureCache: failed to create black stub (0x%08X)\n",
			(unsigned)hr);
		return hr;
	}
	return S_OK;
}

GpuTexture GpuTextureCache::GetOrCreate(DXRender* render, const GameMaterial* mat)
{
	GpuTexture empty;
	if (!render || !mat || mat->source.empty() || mat->width == 0 || mat->height == 0)
		return empty;

	std::string key = MakeKey(mat);
	std::unordered_map<std::string, Entry>::iterator it = m_entries.find(key);
	if (it != m_entries.end())
		return it->second.tex;

	GpuTexture tex;
	HRESULT hr = TextureFactory::CreateSrvFromDxt(
		render,
		mat->Data(),
		mat->source.size(),
		mat->width,
		mat->height,
		mat->dxtCompression,
		&tex
	);
	if (FAILED(hr) || !tex.Valid()) {
		printf("[Warn] GpuTextureCache: upload failed for '%s'\n", mat->name);
		return empty;
	}

	Entry e;
	e.tex = tex;
	e.isBlackStub = false;
	m_entries[key] = e;
	m_owned.push_back(tex);
	return tex;
}

GpuTexture GpuTextureCache::ResolveOrBlack(DXRender* render, const GameMaterial* mat)
{
	EnsureBlack(render);

	if (mat) {
		GpuTexture tex = GetOrCreate(render, mat);
		if (tex.Valid())
			return tex;
	}

	if (m_black.Valid())
		return m_black;

	GpuTexture empty;
	return empty;
}

void GpuTextureCache::Clear(DXRender* render)
{
	(void)render;
	for (size_t i = 0; i < m_owned.size(); i++) {
		if (m_owned[i].resource) {
			m_owned[i].resource->Release();
			m_owned[i].resource = nullptr;
			m_owned[i].srvIndex = UINT_MAX;
		}
	}
	m_owned.clear();
	m_entries.clear();

	if (m_black.resource) {
		m_black.resource->Release();
		m_black.resource = nullptr;
		m_black.srvIndex = UINT_MAX;
	}
}
