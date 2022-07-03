#include "renderware.h"

using namespace std;

namespace rw {

/* you can write ONE header per C BLOCk using these macros */

#define SKIP_HEADER()\
	uint32 bytesWritten = 0;\
	uint32 headerPos = rw.tellp();\
	rw.seekp(0x0C, ios::cur);

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
	uint32 end;
	header.read(rw);
	end = header.length + rw.tellg();

	READ_HEADER(CHUNK_STRUCT);

	uint32 n = readUInt32(rw);
	animList.resize(n);
	for(uint32 i = 0; i < n; i++)
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

}
