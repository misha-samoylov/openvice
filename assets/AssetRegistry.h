#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

#include "assets/GameMaterial.h"
#include "Model.h"
#include "loaders/IDE.hpp"
#include "loaders/IPL.hpp"
#include "loaders/COL.hpp"

class AssetRegistry
{
public:
	std::vector<std::unique_ptr<Model>>& Models() { return m_models; }
	const std::vector<std::unique_ptr<Model>>& Models() const { return m_models; }

	std::vector<GameMaterial>& Textures() { return m_textures; }
	const std::vector<GameMaterial>& Textures() const { return m_textures; }

	std::vector<std::unique_ptr<IDE>>& IdeFiles() { return m_ideFiles; }
	std::vector<std::unique_ptr<IPL>>& IplFiles() { return m_iplFiles; }

	COL* Col() { return m_col.get(); }
	const COL* Col() const { return m_col.get(); }
	void SetCol(std::unique_ptr<COL> col) { m_col = std::move(col); }

	void AddModel(std::unique_ptr<Model> model)
	{
		if (!model)
			return;
		m_modelsById[model->GetId()] = model.get();
		m_models.push_back(std::move(model));
	}

	void RemoveLastModel()
	{
		if (m_models.empty())
			return;
		Model* m = m_models.back().get();
		m_modelsById.erase(m->GetId());
		m_models.pop_back();
	}

	Model* FindModelById(int id) const
	{
		std::unordered_map<int, Model*>::const_iterator it = m_modelsById.find(id);
		return it != m_modelsById.end() ? it->second : nullptr;
	}

	void RebuildIdIndex()
	{
		m_modelsById.clear();
		for (size_t i = 0; i < m_models.size(); i++)
			m_modelsById[m_models[i]->GetId()] = m_models[i].get();
	}

	int FindTextureIndex(const char* name) const
	{
		if (!name)
			return -1;
		for (int i = 0; i < (int)m_textures.size(); i++) {
			if (strcmp(m_textures[i].name, name) == 0)
				return i;
		}
		return -1;
	}

	void AddTexture(GameMaterial mat)
	{
		m_textures.push_back(std::move(mat));
	}

	void Clear()
	{
		m_models.clear();
		m_modelsById.clear();
		m_textures.clear();
		m_ideFiles.clear();
		m_iplFiles.clear();
		m_col.reset();
	}

private:
	std::vector<std::unique_ptr<Model>> m_models;
	std::unordered_map<int, Model*> m_modelsById;
	std::vector<GameMaterial> m_textures;
	std::vector<std::unique_ptr<IDE>> m_ideFiles;
	std::vector<std::unique_ptr<IPL>> m_iplFiles;
	std::unique_ptr<COL> m_col;
};

template <typename T>
inline void RemoveDuplicates(std::vector<T>& vec)
{
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}
