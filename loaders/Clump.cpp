#include "Clump.h"

std::vector<Geometry> Clump::getGeometryList() 
{ 
	return m_geometryList; 
}

std::vector<Light> Clump::getLightList() 
{ 
	return m_lightList;
}

std::vector<Frame> Clump::getFrameList() 
{ 
	return m_frameList; 
}

std::vector<Atomic> Clump::getAtomicList() 
{ 
	return m_atomicList; 
}

void Clump::Read(istream& rw)
{
	HeaderInfo header;
	header.read(rw);

	READ_HEADER(CHUNK_STRUCT);
	uint32_t numAtomics = readUInt32(rw);
	uint32_t numLights = 0;
	if (header.length == 0xC) {
		numLights = readUInt32(rw);
		rw.seekg(4, ios::cur); /* camera count, unused in gta */
	}
	m_atomicList.resize(numAtomics);

	READ_HEADER(CHUNK_FRAMELIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32_t numFrames = readUInt32(rw);
	m_frameList.resize(numFrames);
	for (uint32_t i = 0; i < numFrames; i++)
		m_frameList[i].readStruct(rw);
	for (uint32_t i = 0; i < numFrames; i++)
		m_frameList[i].readExtension(rw);

	READ_HEADER(CHUNK_GEOMETRYLIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32_t numGeometries = readUInt32(rw);
	m_geometryList.resize(numGeometries);
	for (uint32_t i = 0; i < numGeometries; i++)
		m_geometryList[i].read(rw);

	/* read atomics */
	for (uint32_t i = 0; i < numAtomics; i++)
		m_atomicList[i].read(rw);

	/* read lights */
	m_lightList.resize(numLights);
	for (uint32_t i = 0; i < numLights; i++) {
		READ_HEADER(CHUNK_STRUCT);
		m_lightList[i].frameIndex = readInt32(rw);
		m_lightList[i].read(rw);
	}
	m_hasCollision = false;

	ReadExtension(rw);
}

void Clump::ReadExtension(istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	streampos end = rw.tellg();
	end += header.length;

	while (rw.tellg() < end) {
		header.read(rw);
		switch (header.type) {
		case CHUNK_COLLISIONMODEL:
			m_hasCollision = true;
			m_colData.resize(header.length);
			rw.read((char*)&m_colData[0], header.length);
			break;
		default:
			rw.seekg(header.length, ios::cur);
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