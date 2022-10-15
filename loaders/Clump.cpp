#include "Clump.h"

std::vector<Geometry> Clump::GetGeometryList() 
{ 
	return m_geometryList; 
}

std::vector<Light> Clump::GetLightList() 
{ 
	return m_lightList;
}

std::vector<Frame> Clump::GetFrameList() 
{ 
	return m_frameList; 
}

std::vector<Atomic> Clump::GetAtomicList() 
{ 
	return m_atomicList; 
}

void Clump::Read(char *bytes)
{
	size_t offset = 0;

	HeaderInfo header;
	header.read(bytes, &offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, &offset);

	uint32_t numAtomics = readUInt32(bytes, &offset);
	uint32_t numLights = 0;
	if (header.length == 0xC) {
		numLights = readUInt32(bytes, &offset);
		//rw.seekg(4, ios::cur); /* camera count, unused in gta */
		offset += 4;
		
	}


	m_atomicList.resize(numAtomics);


	//READ_HEADER(CHUNK_FRAMELIST);
	header.read(bytes, &offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, &offset);
	uint32_t numFrames = readUInt32(bytes, &offset);
	m_frameList.resize(numFrames);
	for (uint32_t i = 0; i < numFrames; i++)
		m_frameList[i].readStruct(bytes, &offset);
	for (uint32_t i = 0; i < numFrames; i++)
		m_frameList[i].readExtension(bytes, &offset);

	//READ_HEADER(CHUNK_GEOMETRYLIST);
	header.read(bytes, &offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, &offset);

	uint32_t numGeometries = readUInt32(bytes, &offset);
	m_geometryList.resize(numGeometries);
	for (uint32_t i = 0; i < numGeometries; i++)
		m_geometryList[i].read(bytes, &offset);

	/* read atomics */
	for (uint32_t i = 0; i < numAtomics; i++)
		m_atomicList[i].read(bytes, &offset);

	/* read lights */
	m_lightList.resize(numLights);
	for (uint32_t i = 0; i < numLights; i++) {
		//READ_HEADER(CHUNK_STRUCT);
		header.read(bytes, &offset);

		m_lightList[i].frameIndex = readInt32(bytes, &offset);
		m_lightList[i].read(bytes, &offset);
	}
	m_hasCollision = false;

	ReadExtension(bytes, &offset);
}

void Clump::ReadExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;

	//READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_COLLISIONMODEL:
			m_hasCollision = true;
			m_colData.resize(header.length);
			//rw.read((char*)&m_colData[0], header.length);
			memcpy((char*)&m_colData[0],
				&bytes[*offset],
				header.length);
			*offset += header.length;
			break;
		default:
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		}
	}
}

void Clump::Dump(bool detailed)
{
	string ind = "";
	cout << ind << "Clump {\n";
	ind += "  ";
	cout << ind << "numAtomics: " << m_atomicList.size() << endl;

	cout << endl << ind << "FrameList {\n";
	ind += "  ";
	cout << ind << "numFrames: " << m_frameList.size() << endl;
	for (uint32_t i = 0; i < m_frameList.size(); i++)
		m_frameList[i].dump(i, ind);
	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";

	cout << endl << ind << "GeometryList {\n";
	ind += "  ";
	cout << ind << "numGeometries: " << m_geometryList.size() << endl;
	for (uint32_t i = 0; i < m_geometryList.size(); i++)
		m_geometryList[i].dump(i, ind, detailed);

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n\n";

	for (uint32_t i = 0; i < m_atomicList.size(); i++)
		m_atomicList[i].dump(i, ind);

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";
}

void Clump::Clear(void)
{
	m_atomicList.clear();
	m_geometryList.clear();
	m_frameList.clear();
	m_lightList.clear();
	m_colData.clear();
}