#pragma once

#include "assets/GameMaterial.h"

#include <vector>
#include <string>

/*
 * re3/miami CTxdStore analogue for openvice.
 * TXDs are named slots; model load sets the current slot so materials
 * resolve textures from that dictionary (then parent chain), like
 * CTxdStore::SetCurrentTxd + RwTexDictionaryFindNamedTexture.
 */
struct TxdDef
{
	char name[24];
	int parentSlot;
	int refCount;
	std::vector<GameMaterial> textures;

	TxdDef()
		: parentSlot(-1)
		, refCount(0)
	{
		name[0] = '\0';
	}
};

class TxdStore
{
public:
	TxdStore()
		: m_currentSlot(-1)
		, m_storedSlot(-1)
	{
	}

	void Clear();

	int AddTxdSlot(const char* name);
	int FindTxdSlot(const char* name) const;
	int FindOrAddTxdSlot(const char* name);

	TxdDef* GetSlot(int slot);
	const TxdDef* GetSlot(int slot) const;
	int GetSlotCount() const { return (int)m_slots.size(); }

	void SetParentTxd(int slot, int parentSlot);
	void LinkParentsToGeneric();

	void PushCurrentTxd();
	void PopCurrentTxd();
	void SetCurrentTxd(int slot);
	void SetCurrentTxdByName(const char* name);
	int GetCurrentTxd() const { return m_currentSlot; }

	void AddTexture(int slot, GameMaterial mat);

	/* Case-insensitive. Searches current TXD, then parent chain. */
	GameMaterial* FindTexture(const char* name);
	const GameMaterial* FindTexture(const char* name) const;

	GameMaterial* FindTextureInSlot(int slot, const char* name);
	const GameMaterial* FindTextureInSlot(int slot, const char* name) const;

	/* Flat fallback across all slots (last resort). */
	GameMaterial* FindTextureAnywhere(const char* name);

private:
	static bool NameEquals(const char* a, const char* b);

	std::vector<TxdDef> m_slots;
	int m_currentSlot;
	int m_storedSlot;
};
