#include "LightList.h"

void Light::Read(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_LIGHT */
	header.read(bytes, offset); /* CHUNK_STRUCT */

	radius = readFloat32(bytes, offset);

	memcpy((char*)&color[0], &bytes[*offset], 12);
	*offset += 12;

	minusCosAngle = readFloat32(bytes, offset);
	flags = readUInt16(bytes, offset);
	type = readUInt16(bytes, offset);

	header.read(bytes, offset); /* CHUNK_EXTENSION */

	*offset += header.length;
}

void LightList::Read(uint32_t numLights, char *bytes, size_t *offset)
{
	HeaderInfo header;

	m_numLights = numLights;
	m_lightList = new Light * [numLights];

	for (uint32_t i = 0; i < numLights; i++) {
		m_lightList[i] = new Light();
	}

	for (uint32_t i = 0; i < numLights; i++) {
		header.read(bytes, offset); /* CHUNK_STRUCT */
		m_lightList[i]->frameIndex = readInt32(bytes, offset);
		m_lightList[i]->Read(bytes, offset);
	}
}

void LightList::Cleanup()
{
	for (int i = 0; i < m_numLights; i++) {
		delete m_lightList[i];
	}
	delete[] m_lightList;
}

Light** LightList::GetLightList()
{
	return m_lightList;
}
