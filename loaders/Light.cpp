#include "Light.h"

void Light::read(char* bytes, size_t* offset)
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
