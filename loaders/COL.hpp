#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "collision/ColTypes.h"
#include "loaders/IMG.hpp"

class COL
{
public:
	COL() {}
	~COL() { Cleanup(); }
	COL(const COL&) = delete;
	COL& operator=(const COL&) = delete;

	/* Load every *.col archive from gta3.img into models-by-name. */
	bool LoadAllFromIMG(IMG* img);
	/* Load one COL1 archive buffer (also used by map .col dumps). */
	bool LoadCollisionFile(const uint8_t* data, int32_t size);
	void Cleanup();

	ColModel* FindByName(const char* name);

	size_t GetModelCount() const { return m_models.size(); }

private:
	bool LoadCollisionModel(const uint8_t* buf, uint32_t dataSize, ColModel& model);

	std::unordered_map<std::string, ColModel*> m_models;
	std::vector<ColModel*> m_owned;
};
