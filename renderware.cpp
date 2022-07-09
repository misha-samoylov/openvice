#include <cstdlib>

#include "renderware.h"

bool HeaderInfo::read(istream &rw)
{
	uint32_t buf[3];
	rw.read((char*)buf, 12);
	if(rw.eof())
		return false;
	type = buf[0];
	length = buf[1];
	build = buf[2];
	if(build & 0xFFFF0000)
		version = ((build >> 14) & 0x3FF00) |
		          ((build >> 16) & 0x3F) |
		          0x30000;
	else
		version = build << 8;
	return true;
}

bool HeaderInfo::peek(istream &rw)
{
	if(!read(rw))
		return false;
	rw.seekg(-12, ios::cur);
	return true;
}

bool HeaderInfo::findChunk(istream &rw, uint32_t type)
{
	while(read(rw)){
		if(this->type == CHUNK_NAOBJECT)
			return false;
		if(this->type == type)
			return true;
		rw.seekg(length, ios::cur);
	}
	return false;
}

void ChunkNotFound(CHUNK_TYPE chunk, uint32_t address)
{
	cerr << "chunk " << hex << chunk << " not found at 0x";
	cerr << hex << address << endl;
	exit(1);
}

int8_t readInt8(istream &rw)
{
	int8_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int8_t));
	return tmp;
}

uint8_t readUInt8(istream &rw)
{
	uint8_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint8_t));
	return tmp;
}

int16_t readInt16(istream &rw)
{
	int16_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int16_t));
	return tmp;
}

uint16_t readUInt16(istream &rw)
{
	uint16_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint16_t));
	return tmp;
}

int32_t readInt32(istream &rw)
{
	int32_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int32_t));
	return tmp;
}

uint32_t readUInt32(istream &rw)
{
	uint32_t tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint32_t));
	return tmp;
}

float readFloat32(istream &rw)
{
	float tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(float));
	return tmp;
}
