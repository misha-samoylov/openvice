/* 
 * IPL file. Item placement file.
 * 
 * That file contains model transformation.
 */

#ifndef IPL_H
#define IPL_H

#define MAX_LENGTH_FILENAME 24

#include <stdio.h>
#include <vector>

struct IPLFile {
	int id;
	char modelName[MAX_LENGTH_FILENAME];
	int interior;
	float x, y, z;
	float scale[3];
	float rotation[4];
};

class IPL
{
public:
	int m_countObjectsInMap = 0;
	std::vector<IPLFile> m_MapObjects;

	int GetCountObjects() { return m_countObjectsInMap; }
	void Load(const char* filepath);
};

#endif