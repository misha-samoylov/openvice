#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "collision/ColTypes.h"
#include "loaders/IMG.hpp"

class COL
{
public:
	/* Load every *.col archive from gta3.img into models-by-name. */
	bool LoadAllFromIMG(IMG* img);
	void Cleanup();

	ColModel* FindByName(const char* name);

	size_t GetModelCount() const { return m_models.size(); }

private:
	bool LoadCollisionFile(const uint8_t* data, int32_t size);
	bool LoadCollisionModel(const uint8_t* buf, uint32_t dataSize, ColModel& model);

	std::unordered_map<std::string, ColModel*> m_models;
	std::vector<ColModel*> m_owned;
};
