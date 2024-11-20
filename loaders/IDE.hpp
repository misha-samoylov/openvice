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

struct itemDefinition {
	int objectId;
	char modelName[MAX_LENGTH_FILENAME];
	char textureArchiveName[MAX_LENGTH_FILENAME];
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