/* 
 * IPL file. Item placement file.
 * 
 * That file contains model transformation.
 * File name doesn't contain extension ".dff".
 */

#ifndef IPL_H
#define IPL_H

#define MAX_LENGTH_FILENAME 24
#define MAX_LENGTH_LINE 512

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mapItem {
	int id;
	char modelName[MAX_LENGTH_FILENAME];
	int interior;
	float x, y, z;
	float scale[3];
	float rotation[4];
};

class IPL
{
private:
	int m_countItems = 0;
	struct mapItem* m_mapItems;

public:
	struct mapItem GetItem(int index) { return m_mapItems[index]; }
	int GetCountObjects() { return m_countItems; }
	int Load(const char* filepath);
	void Cleanup() { free(m_mapItems); };
};

#endif
