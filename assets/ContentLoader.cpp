#include "assets/ContentLoader.h"
#include "assets/DffLoader.h"
#include "core/GameConfig.h"
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

	char* fileBuffer = (char*)img->GetFileById(fileId);
	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);

	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture& t = txd.texList[i];
		GameMaterial m;
		memcpy(m.name, t.name, sizeof(t.name));

		size_t len = t.dataSizes[0];
		m.source.assign(t.texels[0], t.texels[0] + len);
		m.width = t.width[0];
		m.height = t.height[0];
		m.dxtCompression = t.dxtCompression;
		m.depth = t.depth;
		m.isAlpha = t.IsAlpha;

		assets.AddTexture(std::move(m));
	}
}

bool ContentLoader::LoadMapContent(IMG* img, DXRender* render, AssetRegistry& assets)
{
	for (int i = 0; i < MapCount(); i++) {
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

	for (size_t i = 0; i < textures.size(); i++)
		LoadTexturesFromTxd(img, assets, textures[i].c_str());

	int skippedShadows = 0;
	for (size_t i = 0; i < assets.IdeFiles().size(); i++) {
		IDE* ide = assets.IdeFiles()[i].get();
		for (int j = 0; j < ide->GetCountItems(); j++) {
			itemDefinition* itemDef = &ide->GetItems()[j];
			if (itemDef->IsShadowModel()) {
				skippedShadows++;
				continue;
			}

			size_t before = assets.Models().size();
			DffLoader::LoadMapDff(img, render, assets, itemDef->modelName, itemDef->objectId);
			if (assets.Models().size() > before) {
				Model* loaded = assets.Models().back().get();
				loaded->SetTimed(itemDef->isTimed, itemDef->timeOn, itemDef->timeOff);
				if (!loaded->IsTimed()) {
					if (DffLoader::IsNightModelName(itemDef->modelName))
						loaded->SetTimed(true, 21, 5);
					else if (DffLoader::IsDayModelName(itemDef->modelName))
						loaded->SetTimed(true, 5, 21);
				}
				if (!loaded->IsVisibleAtHour(WORLD_HOUR)) {
					loaded->Cleanup();
					assets.RemoveLastModel();
				}
			}
		}
	}

	if (skippedShadows > 0)
		printf("[Info] Skipped %d baked shadow models (IDE flag 0x40)\n", skippedShadows);

	DffLoader::EnsureDayNightModelTimes(assets);

	for (int i = 0; i < MapCount(); i++) {
		std::unique_ptr<IPL> ipl(new IPL());
		char path[512];
		sprintf(path, "%s%s/%s.ipl", GTA_VC_MAPS_DIR, kMapNames[i], kMapNames[i]);
		ipl->Load(path);
		assets.IplFiles().push_back(std::move(ipl));
	}

	return true;
}
