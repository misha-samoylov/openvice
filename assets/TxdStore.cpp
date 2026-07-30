#include "assets/TxdStore.h"

#include <stdio.h>
#include <string.h>

void TxdStore::Clear()
{
	m_slots.clear();
	m_currentSlot = -1;
	m_storedSlot = -1;
}

bool TxdStore::NameEquals(const char* a, const char* b)
{
	if (!a || !b)
		return false;
	return _stricmp(a, b) == 0;
}

int TxdStore::AddTxdSlot(const char* name)
{
	if (!name || !name[0])
		return -1;

	TxdDef def;
	strncpy(def.name, name, sizeof(def.name) - 1);
	def.name[sizeof(def.name) - 1] = '\0';
	m_slots.push_back(def);
	return (int)m_slots.size() - 1;
}

int TxdStore::FindTxdSlot(const char* name) const
{
	if (!name || !name[0])
		return -1;
	for (int i = 0; i < (int)m_slots.size(); i++) {
		if (NameEquals(m_slots[i].name, name))
			return i;
	}
	return -1;
}

int TxdStore::FindOrAddTxdSlot(const char* name)
{
	int slot = FindTxdSlot(name);
	if (slot >= 0)
		return slot;
	return AddTxdSlot(name);
}

TxdDef* TxdStore::GetSlot(int slot)
{
	if (slot < 0 || slot >= (int)m_slots.size())
		return nullptr;
	return &m_slots[slot];
}

const TxdDef* TxdStore::GetSlot(int slot) const
{
	if (slot < 0 || slot >= (int)m_slots.size())
		return nullptr;
	return &m_slots[slot];
}

void TxdStore::SetParentTxd(int slot, int parentSlot)
{
	TxdDef* def = GetSlot(slot);
	if (!def)
		return;
	if (parentSlot == slot)
		return;
	def->parentSlot = parentSlot;
}

void TxdStore::LinkParentsToGeneric()
{
	int generic = FindTxdSlot("generic");
	if (generic < 0)
		return;
	for (int i = 0; i < (int)m_slots.size(); i++) {
		if (i == generic)
			continue;
		if (m_slots[i].parentSlot < 0)
			m_slots[i].parentSlot = generic;
	}
}

void TxdStore::PushCurrentTxd()
{
	m_storedSlot = m_currentSlot;
}

void TxdStore::PopCurrentTxd()
{
	m_currentSlot = m_storedSlot;
	m_storedSlot = -1;
}

void TxdStore::SetCurrentTxd(int slot)
{
	m_currentSlot = slot;
}

void TxdStore::SetCurrentTxdByName(const char* name)
{
	m_currentSlot = FindOrAddTxdSlot(name);
}

void TxdStore::AddTexture(int slot, GameMaterial mat)
{
	TxdDef* def = GetSlot(slot);
	if (!def)
		return;
	def->textures.push_back(std::move(mat));
}

GameMaterial* TxdStore::FindTextureInSlot(int slot, const char* name)
{
	TxdDef* def = GetSlot(slot);
	if (!def || !name || !name[0])
		return nullptr;
	for (size_t i = 0; i < def->textures.size(); i++) {
		if (NameEquals(def->textures[i].name, name))
			return &def->textures[i];
	}
	return nullptr;
}

const GameMaterial* TxdStore::FindTextureInSlot(int slot, const char* name) const
{
	return const_cast<TxdStore*>(this)->FindTextureInSlot(slot, name);
}

GameMaterial* TxdStore::FindTexture(const char* name)
{
	if (!name || !name[0])
		return nullptr;

	int slot = m_currentSlot;
	int guard = 0;
	while (slot >= 0 && guard++ < 32) {
		GameMaterial* m = FindTextureInSlot(slot, name);
		if (m)
			return m;
		TxdDef* def = GetSlot(slot);
		if (!def)
			break;
		slot = def->parentSlot;
	}
	return nullptr;
}

const GameMaterial* TxdStore::FindTexture(const char* name) const
{
	return const_cast<TxdStore*>(this)->FindTexture(name);
}

GameMaterial* TxdStore::FindTextureAnywhere(const char* name)
{
	if (!name || !name[0])
		return nullptr;
	for (int i = 0; i < (int)m_slots.size(); i++) {
		GameMaterial* m = FindTextureInSlot(i, name);
		if (m)
			return m;
	}
	return nullptr;
}
