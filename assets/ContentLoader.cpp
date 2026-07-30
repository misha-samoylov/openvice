#include "assets/ContentLoader.h"
#include "assets/DffLoader.h"
#include "core/GameConfig.h"
#include "graphics/GpuTextureCache.h"
#include "renderware.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <assert.h>

namespace
{
	const char* kMapNames[GTA_VC_MAP_COUNT] = {
		"airport", "airportN", "bank", "bar", "bridge", "cisland", "club",
		"concerth", "docks", "downtown", "downtows", "golf", "haiti", "haitiN",
		"hotel", "islandsf", "lawyers", "littleha", "mall", "mansion", "nbeach",
		"nbeachbt", "nbeachw", "oceandn", "oceandrv", "stadint", "starisl",
		"stripclb", "washintn", "washints", "yacht"
	};

#if GTA_VC_IPL_FILTER_ENABLED
#if ENABLE_SINGLE_OBJECT_RT_DEMO
	const char* kIplAllow[] = { "cisland", "golf" };
#else
	/* Temporary allowlist — add/remove basenames as needed. */
	const char* kIplAllow[] = { "starisl", "golf" };
#endif
	const int kIplAllowCount = (int)(sizeof(kIplAllow) / sizeof(kIplAllow[0]));

	bool IsIplAllowed(const char* name)
	{
		for (int i = 0; i < kIplAllowCount; i++) {
			if (_stricmp(name, kIplAllow[i]) == 0)
				return true;
		}
		return false;
	}
#else
	bool IsIplAllowed(const char* name)
	{
		(void)name;
		return true;
	}
#endif
}

const char* const* ContentLoader::MapNames()
{
	return kMapNames;
}

int ContentLoader::MapCount()
{
	return GTA_VC_MAP_COUNT;
}

void ContentLoader::LoadTexturesFromTxd(IMG* img, AssetRegistry& assets, const char* filename)
{
	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, filename);
	strcat(result_name, ".txd");

	int fileId = img->GetFileIndexByName(result_name);
	if (fileId == -1)
		return;

	int slot = assets.Txd().FindOrAddTxdSlot(filename);

	char* fileBuffer = (char*)img->GetFileById(fileId);
	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);

	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture& t = txd.texList[i];
		/* Uncompressed / paletted TXD must not be wrapped as fake DXT. */
		if (t.dxtCompression == 0)
			t.convertTo32Bit();

		GameMaterial m;
		memcpy(m.name, t.name, sizeof(m.name));
		m.name[sizeof(m.name) - 1] = '\0';

		size_t len = t.dataSizes[0];
		m.source.assign(t.texels[0], t.texels[0] + len);
		m.width = t.width[0];
		m.height = t.height[0];
		m.dxtCompression = t.dxtCompression;
		m.depth = t.depth;
		m.isAlpha = t.IsAlpha;

		/* Keep flat list for legacy FindTextureIndex; store also in TXD slot. */
		assets.AddTexture(m);
		assets.Txd().AddTexture(slot, std::move(m));
	}
}

bool ContentLoader::LoadMapContent(IMG* img, DXRender* render, AssetRegistry& assets)
{
	GpuTextureCache::Instance().EnsureBlack(render);

	for (int i = 0; i < MapCount(); i++) {
#if GTA_VC_IPL_FILTER_ENABLED
		/* Only load IDE for allowed maps — avoids uploading unused DFFs. */
		if (!IsIplAllowed(kMapNames[i]))
			continue;
#endif
		std::unique_ptr<IDE> ide(new IDE());
		char path[512];
		sprintf(path, "%s%s/%s.ide", GTA_VC_MAPS_DIR, kMapNames[i], kMapNames[i]);
		int res = ide->Load(path);
		assert(res == 0);
		assets.IdeFiles().push_back(std::move(ide));
	}

	{
		std::unique_ptr<IDE> ide(new IDE());
		int res = ide->Load(GTA_VC_GENERIC_IDE);
		assert(res == 0);
		assets.IdeFiles().push_back(std::move(ide));
	}

	std::vector<std::string> textures;
	for (size_t i = 0; i < assets.IdeFiles().size(); i++) {
		IDE* ide = assets.IdeFiles()[i].get();
		for (int j = 0; j < ide->GetCountItems(); j++) {
			itemDefinition* item = &ide->GetItems()[j];
			textures.push_back(item->textureArchiveName);
		}
	}
	RemoveDuplicates(textures);

	/* Always try generic — parent dictionary for map TXDs (re3-style). */
	LoadTexturesFromTxd(img, assets, "generic");

	for (size_t i = 0; i < textures.size(); i++) {
		if (_stricmp(textures[i].c_str(), "generic") == 0)
			continue;
		LoadTexturesFromTxd(img, assets, textures[i].c_str());
	}

	assets.Txd().LinkParentsToGeneric();

	int skippedShadows = 0;
	int skippedTimed = 0;
	for (size_t i = 0; i < assets.IdeFiles().size(); i++) {
		IDE* ide = assets.IdeFiles()[i].get();
		for (int j = 0; j < ide->GetCountItems(); j++) {
			itemDefinition* itemDef = &ide->GetItems()[j];
			if (itemDef->IsShadowModel()) {
				skippedShadows++;
				continue;
			}

			/*
			 * Skip models that are invisible at WORLD_HOUR before uploading GPU
			 * data. Destroying buffers while the init upload list is still open
			 * triggers D3D12 OBJECT_DELETED_WHILE_STILL_IN_USE (#921).
			 */
			bool visible = true;
			if (itemDef->isTimed) {
				visible = itemDef->IsVisibleAtHour(WORLD_HOUR);
			} else if (DffLoader::IsNightModelName(itemDef->modelName)) {
				visible = (WORLD_HOUR >= 21 || WORLD_HOUR < 5);
			} else if (DffLoader::IsDayModelName(itemDef->modelName)) {
				visible = (WORLD_HOUR >= 5 && WORLD_HOUR < 21);
			}
			if (!visible) {
				skippedTimed++;
				continue;
			}

			/* re3: CTxdStore::SetCurrentTxd(mi->GetTxdSlot()) before LoadAtomicFile */
			assets.Txd().PushCurrentTxd();
			assets.Txd().SetCurrentTxdByName(itemDef->textureArchiveName);

			size_t before = assets.Models().size();
			DffLoader::LoadMapDff(img, render, assets, itemDef->modelName, itemDef->objectId);

			assets.Txd().PopCurrentTxd();

			if (assets.Models().size() > before) {
				Model* loaded = assets.Models().back().get();
				loaded->SetTimed(itemDef->isTimed, itemDef->timeOn, itemDef->timeOff);
				if (!loaded->IsTimed()) {
					if (DffLoader::IsNightModelName(itemDef->modelName))
						loaded->SetTimed(true, 21, 5);
					else if (DffLoader::IsDayModelName(itemDef->modelName))
						loaded->SetTimed(true, 5, 21);
				}
			}
		}
	}

	if (skippedShadows > 0)
		printf("[Info] Skipped %d baked shadow models (IDE flag 0x40)\n", skippedShadows);
	if (skippedTimed > 0)
		printf("[Info] Skipped %d timed models not visible at hour %d\n", skippedTimed, WORLD_HOUR);

	DffLoader::EnsureDayNightModelTimes(assets);

	for (int i = 0; i < MapCount(); i++) {
		if (!IsIplAllowed(kMapNames[i]))
			continue;
		std::unique_ptr<IPL> ipl(new IPL());
		char path[512];
		sprintf(path, "%s%s/%s.ipl", GTA_VC_MAPS_DIR, kMapNames[i], kMapNames[i]);
		ipl->Load(path);
		assets.IplFiles().push_back(std::move(ipl));
	}

#if GTA_VC_IPL_FILTER_ENABLED
	printf("[Info] IPL filter active (%d names) — loaded %u file(s):",
		kIplAllowCount, (unsigned)assets.IplFiles().size());
	for (int i = 0; i < kIplAllowCount; i++)
		printf(" %s", kIplAllow[i]);
	printf("\n");
#endif

	printf("[Info] TxdStore: %d TXD slots, black stub ready=%d\n",
		assets.Txd().GetSlotCount(),
		GpuTextureCache::Instance().HasBlack() ? 1 : 0);

	return true;
}
