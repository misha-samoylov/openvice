#include "Clump.h"

std::vector<Geometry> Clump::GetGeometryList() 
{ 
	return m_geometryList; 
}

LightList *Clump::GetLightList() 
{
	return m_lightList;
}

FrameList *Clump::GetFrameList()
{ 
	return m_frameList; 
}

AtomicList* Clump::GetAtomicList()
{ 
	return m_atomicList; 
}

void Clump::Read(char *bytes)
{
	size_t offset;
	HeaderInfo header;
	uint32_t numGeometries;

	offset = 0;

	header.read(bytes, &offset);// CHUNK_CLUMP
	header.read(bytes, &offset); //READ_HEADER(CHUNK_STRUCT);

	m_numAtomics = readUInt32(bytes, &offset);
	m_numLights = 0;
	if (header.length == 0xC) {
		m_numLights = readUInt32(bytes, &offset);
		offset += 4;  /* Camera count, unused in GTA */
	}

	/* Frame list */
	m_frameList = new FrameList();
	m_frameList->Read(bytes, &offset);

	/* Geometries */
	header.read(bytes, &offset); // CHUNK_GEOMETRYLIST
	header.read(bytes, &offset); // CHUNK_STRUCT

	numGeometries = readUInt32(bytes, &offset);
	m_geometryList.resize(numGeometries);

	for (uint32_t i = 0; i < numGeometries; i++)
		m_geometryList[i].read(bytes, &offset);

	/* Atomic */
	m_atomicList = new AtomicList();
	m_atomicList->Read(m_numAtomics, bytes, &offset);

	/* Light */
	m_lightList = new LightList();
	m_lightList->Read(m_numLights, bytes, &offset);

	m_hasCollision = false;

	ReadExtension(bytes, &offset);
}

void Clump::ReadExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;
	header.read(bytes, offset); //READ_HEADER(CHUNK_EXTENSION);

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
	cout << ind << "numAtomics: " << m_numAtomics << endl;

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

	for (uint32_t i = 0; i < m_numAtomics; i++) {
		m_atomicList->GetAtomic(i)->Dump(i);
	}

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";
}

void Clump::Clear(void)
{
	m_atomicList->Cleanup();
	delete m_atomicList;

	m_geometryList.clear();

	m_frameList->Cleanup();
	delete m_frameList;

	m_lightList->Cleanup();
	delete m_lightList;

	m_colData.clear();
}
