#include "Clump.h"

std::vector<Geometry> Clump::GetGeometryList() 
{ 
	return m_geometryList; 
}

Light **Clump::GetLightList() 
{ 
	return m_lightList;
}

FrameList *Clump::GetFrameList()
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
	// CHUNK_CLUMP
	header.read(bytes, &offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, &offset);

	uint32_t numAtomics = readUInt32(bytes, &offset);
	numLights = 0;
	if (header.length == 0xC) {
		numLights = readUInt32(bytes, &offset);
		//rw.seekg(4, ios::cur); /* camera count, unused in gta */
		offset += 4;
		
	}

	// Frame list
	m_frameList = new FrameList();
	m_frameList->Read(bytes, &offset);


	// CHUNK_GEOMETRYLIST
	header.read(bytes, &offset);

	// CHUNK_STRUCT
	header.read(bytes, &offset);

	uint32_t numGeometries = readUInt32(bytes, &offset);
	m_geometryList.resize(numGeometries);

	for (uint32_t i = 0; i < numGeometries; i++)
		m_geometryList[i].read(bytes, &offset);

	m_atomicList.resize(numAtomics);

	/* read atomics */
	for (uint32_t i = 0; i < numAtomics; i++)
		m_atomicList[i].read(bytes, &offset);

	/* Lights */
	m_lightList = new Light * [numLights];

	for (uint32_t i = 0; i < numLights; i++) {
		m_lightList[i] = new Light();
	}

	for (uint32_t i = 0; i < numLights; i++) {
		header.read(bytes, &offset); /* CHUNK_STRUCT */
		m_lightList[i]->frameIndex = readInt32(bytes, &offset);
		m_lightList[i]->read(bytes, &offset);
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

	printf("FrameList\n");
	printf("numFrames: %d\n", m_frameList->GetNumFrames());
	for (uint32_t i = 0; i < m_frameList->GetNumFrames(); i++)
		m_frameList->GetFrame(i)->Dump(i);

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

	m_frameList->Cleanup();
	delete m_frameList;

	// light
	for (int i = 0; i < numLights; i++) {
		delete m_lightList[i];
	}
	delete[] m_lightList;

	m_colData.clear();
}
