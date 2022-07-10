#include "renderware.h"

void UVAnimation::read(char *bytes, size_t *offset)
{
	HeaderInfo header;
	//READ_HEADER(CHUNK_ANIMANIMATION);
	header.read(bytes, offset);

	data.resize(header.length);

	memcpy((char*)&data[0], &bytes[*offset], header.length);
	*offset += sizeof(header.length);
	// rw.read((char*)&data[0], header.length);
}

void UVAnimDict::read(char *bytes, size_t *offset)
{
	HeaderInfo header;
	header.read(bytes, offset);

	uint32_t end;
	end = header.length + *offset;

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	uint32_t n = readUInt32(bytes, offset);
	animList.resize(n);
	for(uint32_t i = 0; i < n; i++)
		animList[i].read(bytes, offset);
}

void UVAnimDict::clear()
{
	animList.clear();
}

UVAnimDict::~UVAnimDict()
{
	animList.clear();
}