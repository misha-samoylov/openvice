#include "renderware.h"

void UVAnimation::read(istream &rw)
{
	HeaderInfo header;
	READ_HEADER(CHUNK_ANIMANIMATION);
	data.resize(header.length);
	rw.read((char*)&data[0], header.length);
}

void UVAnimDict::read(istream &rw)
{
	HeaderInfo header;
	uint32_t end;
	header.read(rw);
	end = header.length + rw.tellg();

	READ_HEADER(CHUNK_STRUCT);

	uint32_t n = readUInt32(rw);
	animList.resize(n);
	for(uint32_t i = 0; i < n; i++)
		animList[i].read(rw);
}

void UVAnimDict::clear(void)
{
	animList.clear();
}

UVAnimDict::~UVAnimDict(void)
{
	animList.clear();
}