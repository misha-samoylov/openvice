#include "LightList.h"

void Light::Read(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_LIGHT */
	header.read(bytes, offset); /* CHUNK_STRUCT */

	m_radius = readFloat32(bytes, offset);

	memcpy((char*)&m_color[0], &bytes[*offset], 12);
	*offset += 12;

	m_minusCosAngle = readFloat32(bytes, offset);
	m_flags = readUInt16(bytes, offset);
	m_type = readUInt16(bytes, offset);

	header.read(bytes, offset); /* CHUNK_EXTENSION */

	*offset += header.length;
}

void Light::SetFrameIndex(int32_t frameIndex)
{
	m_frameIndex = frameIndex;
}

void LightList::Read(uint32_t numLights, char *bytes, size_t *offset)
{
	HeaderInfo header;
	int32_t frameIndex;

	m_numLights = numLights;
	m_lightList = new Light * [numLights];

	for (uint32_t i = 0; i < numLights; i++) {
		m_lightList[i] = new Light();
	}

	for (uint32_t i = 0; i < numLights; i++) {
		header.read(bytes, offset); /* CHUNK_STRUCT */

		frameIndex = readInt32(bytes, offset);
		m_lightList[i]->SetFrameIndex(frameIndex);
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
