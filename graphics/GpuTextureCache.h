#pragma once

#include "DXRender.hpp"
#include "assets/GameMaterial.h"

#include <unordered_map>
#include <string>
#include <vector>

/*
 * DX12 GPU texture cache.
 * Uploads each CPU GameMaterial once and shares the SRV across meshes
 * (re3 shares RwTexture instances from the current TXD).
 * Missing textures bind a permanent black 1x1 stub.
 */
class GpuTextureCache
{
public:
	static GpuTextureCache& Instance();

	/* Ensure black stub exists. Safe to call repeatedly. */
	HRESULT EnsureBlack(DXRender* render);
	const GpuTexture& Black() const { return m_black; }
	bool HasBlack() const { return m_black.Valid(); }

	/*
	 * Upload (or reuse) GPU texture for material.
	 * Keyed by material identity (name + dimensions + payload size).
	 * On failure returns an invalid GpuTexture.
	 */
	GpuTexture GetOrCreate(DXRender* render, const GameMaterial* mat);

	/* Always returns a valid texture when EnsureBlack succeeded. */
	GpuTexture ResolveOrBlack(DXRender* render, const GameMaterial* mat);

	void Clear(DXRender* render);

private:
	GpuTextureCache() = default;

	struct Entry
	{
		GpuTexture tex;
		bool isBlackStub;
	};

	static std::string MakeKey(const GameMaterial* mat);

	GpuTexture m_black;
	std::unordered_map<std::string, Entry> m_entries;
	std::vector<GpuTexture> m_owned;
};
