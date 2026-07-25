/* 
 * IDE. Item Definition file.
 * 
 * That file contains information in
 * struct: dff_file txd_file.
 */

#ifndef IDE_H
#define IDE_H

#define MAX_LENGTH_FILENAME 24
#define MAX_LENGTH_LINE 512

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

struct itemDefinition {
	int objectId;
	char modelName[MAX_LENGTH_FILENAME];
	char textureArchiveName[MAX_LENGTH_FILENAME];
	uint32_t flags;
	bool isTimed;
	int timeOn;   /* hour 0..24, inclusive start (tobj only) */
	int timeOff;  /* hour 0..24, exclusive end; may wrap overnight */

	/* Like re3 CTimeModelInfo::GetIsTimeVisible. */
	bool IsVisibleAtHour(int hour) const
	{
		if (!isTimed)
			return true;
		if (timeOn > timeOff)
			return hour >= timeOn || hour < timeOff;
		return hour >= timeOn && hour < timeOff;
	}

	/* IDE flag bit 8 — additive (window night-lights, neons). */
	bool IsAdditive() const { return (flags & 8) != 0; }
};

class IDE
{
private:
	int m_countItems = 0;
	struct itemDefinition* m_items;

public:
	int Load(const char* filepath);
	int GetCountItems() { return m_countItems; }
	struct itemDefinition* GetItems() { return m_items; }
	void Cleanup() { free(m_items); }
};

#endif
