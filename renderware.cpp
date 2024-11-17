#include "renderware.h"

bool HeaderInfo::read(char *bytes, size_t *offset)
{
	uint32_t buf[3];

	// rw.read((char*)buf, 12);
	memcpy(buf, &bytes[*offset], sizeof(buf));
	*offset += sizeof(buf);

	//if(rw.eof())
	//	return false;
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

bool HeaderInfo::peek(size_t *offset)
{
	//if(!read(rw))
	//	return false;
	*offset -= 12;
	// rw.seekg(-12, ios::cur);
	return true;
}

bool HeaderInfo::findChunk(char *bytes, size_t *offset, uint32_t type)
{
	while(read(bytes, offset)){
		if(this->type == CHUNK_NAOBJECT)
			return false;
		if(this->type == type)
			return true;

		*offset = length; //  , ios::cur);
	}
	return false;
}

void ChunkNotFound(CHUNK_TYPE chunk, uint32_t address)
{
	std::cerr << "chunk " << std::hex << chunk << " not found at 0x";
	std::cerr << std::hex << address << std::endl;
	exit(1);
}

int8_t readInt8(char *bytes, size_t *offset)
{
	int8_t tmp;
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);

	// rw.read(reinterpret_cast <char *> (&tmp), sizeof(int8_t));
	return tmp;
}

uint8_t readUInt8(char *bytes, size_t *offset)
{
	uint8_t tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint8_t));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}

int16_t readInt16(char *bytes, size_t *offset)
{
	int16_t tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(int16_t));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}

uint16_t readUInt16(char *bytes, size_t *offset)
{
	uint16_t tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint16_t));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}

int32_t readInt32(char *bytes, size_t *offset)
{
	int32_t tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(int32_t));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}

uint32_t readUInt32(char *bytes, size_t *offset)
{
	uint32_t tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint32_t));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}

float readFloat32(char *bytes, size_t *offset)
{
	float tmp;
	//rw.read(reinterpret_cast <char *> (&tmp), sizeof(float));
	memcpy(&tmp, &bytes[*offset], sizeof(tmp));
	*offset += sizeof(tmp);
	return tmp;
}
