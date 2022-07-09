#include "Clump.h"

void Clump::read(istream& rw)
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
	atomicList.resize(numAtomics);

	READ_HEADER(CHUNK_FRAMELIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32_t numFrames = readUInt32(rw);
	frameList.resize(numFrames);
	for (uint32_t i = 0; i < numFrames; i++)
		frameList[i].readStruct(rw);
	for (uint32_t i = 0; i < numFrames; i++)
		frameList[i].readExtension(rw);

	READ_HEADER(CHUNK_GEOMETRYLIST);

	READ_HEADER(CHUNK_STRUCT);
	uint32_t numGeometries = readUInt32(rw);
	geometryList.resize(numGeometries);
	for (uint32_t i = 0; i < numGeometries; i++)
		geometryList[i].read(rw);

	/* read atomics */
	for (uint32_t i = 0; i < numAtomics; i++)
		atomicList[i].read(rw);

	/* read lights */
	lightList.resize(numLights);
	for (uint32_t i = 0; i < numLights; i++) {
		READ_HEADER(CHUNK_STRUCT);
		lightList[i].frameIndex = readInt32(rw);
		lightList[i].read(rw);
	}
	hasCollision = false;

	readExtension(rw);
}

void Clump::readExtension(istream &rw)
{
	HeaderInfo header;

	READ_HEADER(CHUNK_EXTENSION);

	streampos end = rw.tellg();
	end += header.length;

	while (rw.tellg() < end) {
		header.read(rw);
		switch (header.type) {
		case CHUNK_COLLISIONMODEL:
			hasCollision = true;
			colData.resize(header.length);
			rw.read((char*)&colData[0], header.length);
			break;
		default:
			rw.seekg(header.length, ios::cur);
			break;
		}
	}
}

void Clump::dump(bool detailed)
{
	string ind = "";
	cout << ind << "Clump {\n";
	ind += "  ";
	cout << ind << "numAtomics: " << atomicList.size() << endl;

	cout << endl << ind << "FrameList {\n";
	ind += "  ";
	cout << ind << "numFrames: " << frameList.size() << endl;
	for (uint32_t i = 0; i < frameList.size(); i++)
		frameList[i].dump(i, ind);
	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";

	cout << endl << ind << "GeometryList {\n";
	ind += "  ";
	cout << ind << "numGeometries: " << geometryList.size() << endl;
	for (uint32_t i = 0; i < geometryList.size(); i++)
		geometryList[i].dump(i, ind, detailed);

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n\n";

	for (uint32_t i = 0; i < atomicList.size(); i++)
		atomicList[i].dump(i, ind);

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";
}

void Clump::clear(void)
{
	atomicList.clear();
	geometryList.clear();
	frameList.clear();
	lightList.clear();
	colData.clear();
}