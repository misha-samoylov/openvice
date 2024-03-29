#include <cmath>

#include "renderware.h"

void Light::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	// READ_HEADER(CHUNK_LIGHT);
	header.read(bytes, offset);

	// READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	radius = readFloat32(bytes, offset);
	//rw.read((char*)&color[0], 12);
	memcpy((char*)&color[0], &bytes[*offset], 12);
	*offset += 12;

	minusCosAngle = readFloat32(bytes, offset);
	flags = readUInt16(bytes, offset);
	type = readUInt16(bytes, offset);

	// READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	// rw.seekg(header.length, ios::cur);
	*offset += header.length;
}

/*
 * Atomic
 */

void Atomic::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	//READ_HEADER(CHUNK_ATOMIC);
	header.read(bytes, offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	frameIndex = readUInt32(bytes, offset);
	geometryIndex = readUInt32(bytes, offset);
	//rw.seekg(8, ios::cur);	// constant
	*offset += 8;

	readExtension(bytes, offset);
}

void Atomic::readExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;

	// READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_RIGHTTORENDER:
			hasRightToRender = true;
			rightToRenderVal1 = readUInt32(bytes, offset);
			rightToRenderVal2 = readUInt32(bytes, offset);
//cout << filename << " atmrights: " << hex << rightToRenderVal1 << " " << rightToRenderVal2 << endl;
			break;
		case CHUNK_PARTICLES:
			hasParticles = true;
			particlesVal = readUInt32(bytes, offset);
			break;
		case CHUNK_MATERIALEFFECTS:
			hasMaterialFx = true;
			materialFxVal = readUInt32(bytes, offset);
			break;
		case CHUNK_PIPELINESET:
			hasPipelineSet = true;
			pipelineSetVal = readUInt32(bytes, offset);
//cout << filename << " pipelineset " << hex << pipelineSetVal << endl;
			break;
		default:
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		}
	}
}

void Atomic::dump(uint32_t index, string ind)
{
	cout << ind << "Atomic " << index << " {\n";
	ind += "  ";

	cout << ind << "frameIndex: " << frameIndex << endl;
	cout << ind << "geometryIndex: " << geometryIndex << endl;

	if (hasRightToRender) {
		cout << hex;
		cout << ind << "Right to Render {\n";
		cout << ind << ind << "val1: " << rightToRenderVal1<<endl;
		cout << ind << ind << "val2: " << rightToRenderVal2<<endl;
		cout << ind << "}\n";
		cout << dec;
	}

	if (hasParticles)
		cout << ind << "particlesVal: " << particlesVal << endl;

	if (hasPipelineSet)
		cout << ind << "pipelineSetVal: " << pipelineSetVal << endl;

	if (hasMaterialFx)
		cout << ind << "materialFxVal: " << materialFxVal << endl;

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

Atomic::Atomic(void)
: frameIndex(-1), geometryIndex(-1), hasRightToRender(false),
  rightToRenderVal1(0), rightToRenderVal2(0), hasParticles(false),
  particlesVal(0), hasPipelineSet(false), pipelineSetVal(0),
  hasMaterialFx(false), materialFxVal(0)
{
}

/*
 * Frame
 */

// only reads part of the frame struct
void Frame::readStruct(char *bytes, size_t *offset)
{
	//rw.read((char *) rotationMatrix, 9*sizeof(float));
	memcpy((char *)rotationMatrix, &bytes[*offset], 9 * sizeof(float));
	*offset += 9 * sizeof(float);
	
	// rw.read((char *) position, 3*sizeof(float));
	memcpy((char *)position, &bytes[*offset], 3 * sizeof(float));
	*offset += 3 * sizeof(float);

	parent = readInt32(bytes, offset);

	// rw.seekg(4, ios::cur);	// matrix creation flag, unused
	*offset += 4;
}

void Frame::readExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;

	//READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);

		switch (header.type) {
		case CHUNK_FRAME:
		{
			char *buffer = new char[header.length+1];
			//rw.read(buffer, header.length);
			memcpy(buffer, &bytes[*offset], header.length);
			*offset += header.length;

			buffer[header.length] = '\0';
			name = buffer;
			delete[] buffer;
			break;
		}
		case CHUNK_HANIM:
			hasHAnim = true;

			hAnimUnknown1 = readUInt32(bytes, offset);
			hAnimBoneId = readInt32(bytes, offset);;
			hAnimBoneCount = readUInt32(bytes, offset);

			if (hAnimBoneCount != 0) {
				hAnimUnknown2 = readUInt32(bytes, offset);;
				hAnimUnknown3 = readUInt32(bytes, offset);;
			}

			for (uint32_t i = 0; i < hAnimBoneCount; i++) {
				hAnimBoneIds.push_back(readInt32(bytes, offset));
				hAnimBoneNumbers.push_back(readUInt32(bytes, offset));

				uint32_t flag = readUInt32(bytes, offset);

				if((flag&~0x3) != 0)
					cout << flag << endl;

				hAnimBoneTypes.push_back(flag);
			}
			break;
		default:
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		}
	}
//	if(hasHAnim)
//		cout << hAnimBoneId << " " << name << endl;
}

void Frame::dump(uint32_t index, string ind)
{
	cout << ind << "Frame " << index << " {\n";
	ind += "  ";

	cout << ind << "rotationMatrix: ";
	for (uint32_t i = 0; i < 9; i++)
		cout << rotationMatrix[i] << " ";
	cout << endl;

	cout << ind << "position: ";
	for (uint32_t i = 0; i < 3; i++)
		cout << position[i] << " ";
	cout << endl;
	cout << ind << "parent: " << parent << endl;

	cout << ind << "name: " << name << endl;

	// TODO: hanim

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

Frame::Frame(void)
: parent(-1), name(""), hasHAnim(false), hAnimUnknown1(0), hAnimBoneId(-1),
  hAnimBoneCount(0), hAnimUnknown2(0), hAnimUnknown3(0)
{
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		for (int j = 0; j < 3; j++)
			rotationMatrix[i*3+j] = (i == j) ? 1.0f : 0.0f;
	}
}

/*
 * Geometry
 */

void Geometry::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	// READ_HEADER(CHUNK_GEOMETRY);
	header.read(bytes, offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);
	flags = readUInt16(bytes, offset);
	numUVs = readUInt8(bytes, offset);
	if (flags & FLAGS_TEXTURED)
		numUVs = 1;
	hasNativeGeometry = readUInt8(bytes, offset);
	uint32_t triangleCount = readUInt32(bytes, offset);
	vertexCount = readUInt32(bytes, offset);

	//rw.seekg(4, ios::cur); /* number of morph targets, uninteresting */
	*offset += 4;

	// skip light info
	if (header.version < 0x34000)
		// rw.seekg(12, ios::cur);
		*offset += 12;

	if (!hasNativeGeometry) {
		if (flags & FLAGS_PRELIT) {
			vertexColors.resize(4*vertexCount);
			//rw.read((char *) (&vertexColors[0]),
			//         4*vertexCount*sizeof(uint8_t));
			memcpy((char *)(&vertexColors[0]),
				&bytes[*offset],
				4 * vertexCount * sizeof(uint8_t));
			*offset += 4 * vertexCount * sizeof(uint8_t);
		}
		if (flags & FLAGS_TEXTURED) {
			texCoords[0].resize(2*vertexCount);
			//rw.read((char *) (&texCoords[0][0]),
			//         2*vertexCount*sizeof(float));
			memcpy((char *)(&texCoords[0][0]),
				&bytes[*offset],
				2 * vertexCount * sizeof(float));
			*offset += 2 * vertexCount * sizeof(float);

		}
		if (flags & FLAGS_TEXTURED2) {
			for (uint32_t i = 0; i < numUVs; i++) {
				texCoords[i].resize(2*vertexCount);
				//rw.read((char *) (&texCoords[i][0]),
				//	 2*vertexCount*sizeof(float));

				memcpy((char *)(&texCoords[i][0]),
					&bytes[*offset],
					2 * vertexCount * sizeof(float));
				*offset += 2 * vertexCount * sizeof(float);
			}
		}
		faces.resize(4*triangleCount);
		//rw.read((char *) (&faces[0]), 4*triangleCount*sizeof(uint16_t));
		memcpy((char *)(&faces[0]),
			&bytes[*offset],
			4 * triangleCount * sizeof(uint16_t));
		*offset += 4 * triangleCount * sizeof(uint16_t);
	}

	/* morph targets, only 1 in gta */
	//rw.read((char *)(boundingSphere), 4*sizeof(float));
	memcpy((char *)(boundingSphere),
		&bytes[*offset],
		4 * sizeof(float));
	*offset += 4 * sizeof(float);

	//hasPositions = (flags & FLAGS_POSITIONS) ? 1 : 0;
	hasPositions = readUInt32(bytes, offset);
	hasNormals = readUInt32(bytes, offset);
	// need to recompute:
	hasPositions = 1;
	hasNormals = (flags & FLAGS_NORMALS) ? 1 : 0;

	if (!hasNativeGeometry) {
		vertices.resize(3*vertexCount);
		//rw.read((char *) (&vertices[0]), 3*vertexCount*sizeof(float));
		memcpy((char *)(&vertices[0]),
			&bytes[*offset],
			3 * vertexCount * sizeof(float));
		*offset += 3 * vertexCount * sizeof(float);


		if (flags & FLAGS_NORMALS) {
			normals.resize(3*vertexCount);
			//rw.read((char *) (&normals[0]),
			//	 3*vertexCount*sizeof(float));
			memcpy((char *)(&normals[0]),
				&bytes[*offset],
				3 * vertexCount * sizeof(float));
			*offset += 3 * vertexCount * sizeof(float);

		}

	}

	//READ_HEADER(CHUNK_MATLIST);
	header.read(bytes, offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	uint32_t numMaterials = readUInt32(bytes, offset);
	//rw.seekg(numMaterials*4, ios::cur);	// constant
	*offset += numMaterials * 4;

	materialList.resize(numMaterials);
	for (uint32_t i = 0; i < numMaterials; i++)
		materialList[i].read(bytes, offset);

	readExtension(bytes, offset);
}

void Geometry::readExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;

	// READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset;
	end += header.length;

	while(*offset < end){
		header.read(bytes, offset);

		switch(header.type){
		case CHUNK_BINMESH: {
			faceType = readUInt32(bytes, offset);
			uint32_t numSplits = readUInt32(bytes, offset);
			numIndices = readUInt32(bytes, offset);
			splits.resize(numSplits);
			bool hasData = header.length > 12+numSplits*8;
			for(uint32_t i = 0; i < numSplits; i++){
				uint32_t numIndices = readUInt32(bytes, offset);
				splits[i].matIndex = readUInt32(bytes, offset);
				splits[i].indices.resize(numIndices);
				if(hasData){
					/* OpenGL Data */
					if(hasNativeGeometry)
					for(uint32_t j = 0; j < numIndices; j++)
						splits[i].indices[j] = 
						  readUInt16(bytes, offset);
					else
					for(uint32_t j = 0; j < numIndices; j++)
						splits[i].indices[j] = 
						  readUInt32(bytes, offset);
				}
			}
			break;
		} case CHUNK_NATIVEDATA: {
			streampos beg = *offset;
			uint32_t size = header.length;
			uint32_t build = header.build;
			header.read(bytes, offset);

			if(header.build==build && header.type==CHUNK_STRUCT){
				uint32_t platform = readUInt32(bytes, offset);
				//rw.seekg(beg, ios::beg);
				*offset += beg;

				//if(platform == PLATFORM_PS2)
				//	readPs2NativeData(rw);
				//else if(platform == PLATFORM_XBOX)
				//	readXboxNativeData(rw);
				//else
					cout << "unknown platform " <<
					        platform << endl;
			}else{
				//rw.seekg(beg, ios::beg);
				*offset += beg;
				//readOglNativeData(rw, size);
			}
			break;
		}
		case CHUNK_MESHEXTENSION: {
			hasMeshExtension = true;
			meshExtension = new MeshExtension;
			meshExtension->unknown = readUInt32(bytes, offset);
			readMeshExtension(bytes, offset);
			break;
		} case CHUNK_NIGHTVERTEXCOLOR: {
			hasNightColors = true;
			nightColorsUnknown = readUInt32(bytes, offset);
			if(nightColors.size() != 0){
				// native data also has them, so skip
				//rw.seekg(header.length - sizeof(uint32_t),
				//          ios::cur);
				*offset += header.length - sizeof(uint32_t);
			}else{
				if(nightColorsUnknown != 0){
				/* TODO: could be better */
					nightColors.resize(header.length-4);
					//rw.read((char *)
					//   (&nightColors[0]), header.length-4);
					memcpy((char *)(&nightColors[0]),
						&bytes[*offset],
						header.length - 4);
					*offset += header.length - 4;

				}
			}
			break;
		} case CHUNK_MORPH: {
			hasMorph = true;
			/* always 0 */
			readUInt32(bytes, offset);
			break;
		} case CHUNK_SKIN: {
			if(hasNativeGeometry){
				streampos beg = *offset;
				//rw.seekg(0x0c, ios::cur);
				*offset += 0x0c;

				uint32_t platform = readUInt32(bytes, offset);
				//rw.seekg(beg, ios::beg);
				*offset += beg;

//				streampos end = beg+header.length;
				if(platform == PLATFORM_OGL ||
				   platform == PLATFORM_PS2){
					hasSkin = true;
					readNativeSkinMatrices(bytes, offset);
				//}else if(platform == PLATFORM_XBOX){
				//	hasSkin = true;
				//	readXboxNativeSkin(rw);
				}else{
					cout << "skin: unknown platform "
					     << platform << endl;
					//rw.seekg(header.length, ios::cur);
					*offset = header.length;
				}
			}else{
				hasSkin = true;
				boneCount = readUInt8(bytes, offset);
				specialIndexCount = readUInt8(bytes, offset);
				unknown1 = readUInt8(bytes, offset);
				unknown2 = readUInt8(bytes, offset);

				if (specialIndexCount) {
					specialIndices.resize(specialIndexCount);
					//rw.read((char *)(&specialIndices[0]),
					//	specialIndexCount * sizeof(uint8_t));
					memcpy((char *)(&specialIndices[0]),
						&bytes[*offset],
						specialIndexCount * sizeof(uint8_t));
					*offset += specialIndexCount * sizeof(uint8_t);
				}

				vertexBoneIndices.resize(vertexCount);
				//rw.read((char *) (&vertexBoneIndices[0]),
				//	 vertexCount*sizeof(uint32_t));
				memcpy((char *)(&vertexBoneIndices[0]),
					&bytes[*offset],
					vertexCount * sizeof(uint32_t));
				*offset += vertexCount * sizeof(uint32_t);

				vertexBoneWeights.resize(vertexCount*4);
				//rw.read((char *) (&vertexBoneWeights[0]),
				//	 vertexCount*4*sizeof(float));
				memcpy((char *)(&vertexBoneWeights[0]),
					&bytes[*offset],
					vertexCount * 4 * sizeof(float));
				*offset += vertexCount * 4 * sizeof(float);

				inverseMatrices.resize(boneCount*16);
				for(uint32_t i = 0; i < boneCount; i++){
					// skip 0xdeaddead
					if (specialIndexCount == 0)
						//rw.seekg(4, ios::cur);
						*offset += 4;

					//rw.read((char *)(&inverseMatrices[i*0x10]),
					//	 0x10*sizeof(float));
					memcpy((char *)(&inverseMatrices[i * 0x10]),
						&bytes[*offset],
						0x10 * sizeof(float));
					*offset += 0x10 * sizeof(float);
				}
				// skip some zeroes
				if (specialIndexCount != 0)
					//rw.seekg(0x0C, ios::cur);
					*offset += 0x0C;
			}
			break;
		}
		case CHUNK_ADCPLG:
			/* only sa ps2, ignore (not very interesting anyway) */
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		case CHUNK_2DFX:
			has2dfx = true;
			twodfxData.resize(header.length);
			//rw.read((char*)&twodfxData[0], header.length);
			memcpy((char*)&twodfxData[0],
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

void Geometry::readNativeSkinMatrices(char *bytes, size_t *offset)
{
	HeaderInfo header;

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	uint32_t platform = readUInt32(bytes, offset);
	if (platform != PLATFORM_PS2 && platform != PLATFORM_OGL) {
		cerr << "error: native skin not in ps2 or ogl format\n";
		return;
	}

	boneCount = readUInt8(bytes, offset);
	specialIndexCount = readUInt8(bytes, offset);
	unknown1 = readUInt8(bytes, offset);
	unknown2 = readUInt8(bytes, offset);

	specialIndices.resize(specialIndexCount);
	//rw.read((char *) (&specialIndices[0]),
	//		specialIndexCount*sizeof(uint8_t));
	memcpy((char *)(&specialIndices[0]),
		&bytes[*offset],
		specialIndexCount * sizeof(uint8_t));
	*offset += specialIndexCount * sizeof(uint8_t);

	inverseMatrices.resize(boneCount * 0x10);
	for (uint32_t i = 0; i < boneCount; i++) {
	//rw.read((char *) (&inverseMatrices[i*0x10]),
	//        0x10*sizeof(float));
		memcpy((char *)(&inverseMatrices[i * 0x10]),
			&bytes[*offset],
			0x10 * sizeof(float));
		*offset += 0x10 * sizeof(float);
	}

	// skip unknowns
	if (specialIndexCount != 0)
		//rw.seekg(0x1C, ios::cur);
		*offset += 0x1C;
}

void Geometry::readMeshExtension(char *bytes, size_t *offset)
{
	if (meshExtension->unknown == 0)
		return;

	// rw.seekg(0x4, ios::cur);
	*offset += 0x4;
	
	uint32_t vertexCount = readUInt32(bytes, offset);
	//rw.seekg(0xC, ios::cur);
	*offset += 0xC;

	uint32_t faceCount = readUInt32(bytes, offset);
	//rw.seekg(0x8, ios::cur);
	*offset += 0x8;

	uint32_t materialCount = readUInt32(bytes, offset);
	//rw.seekg(0x10, ios::cur);
	*offset += 0x10;

	/* vertices */
	meshExtension->vertices.resize(3*vertexCount);
	//rw.read((char *) (&meshExtension->vertices[0]),
		 //3*vertexCount*sizeof(float));
	memcpy((char *)(&meshExtension->vertices[0]),
		&bytes[*offset],
		3 * vertexCount * sizeof(float));
	*offset += 3 * vertexCount * sizeof(float);

	/* tex coords */
	meshExtension->texCoords.resize(2*vertexCount);
	//rw.read((char *) (&meshExtension->texCoords[0]),
	//	 2*vertexCount*sizeof(float));
	memcpy((char *)(&meshExtension->texCoords[0]),
		&bytes[*offset],
		2 * vertexCount * sizeof(float));
	*offset += 2 * vertexCount * sizeof(float);

	/* vertex colors */
	meshExtension->vertexColors.resize(4*vertexCount);
	//rw.read((char *) (&meshExtension->vertexColors[0]),
	//	 4*vertexCount*sizeof(uint8_t));
	memcpy((char *)(&meshExtension->vertexColors[0]),
		&bytes[*offset],
		4 * vertexCount * sizeof(uint8_t));
	*offset += 4 * vertexCount * sizeof(uint8_t);


	/* faces */
	meshExtension->faces.resize(3*faceCount);
	//rw.read((char *) (&meshExtension->faces[0]),
	//	 3*faceCount*sizeof(uint16_t));
	memcpy((char *)(&meshExtension->faces[0]),
		&bytes[*offset],
		3 * faceCount * sizeof(uint16_t));
	*offset += 3 * faceCount * sizeof(uint16_t);


	/* material assignments */
	meshExtension->assignment.resize(faceCount);
	//rw.read((char *) (&meshExtension->assignment[0]),
	//	 faceCount*sizeof(uint16_t));
	memcpy((char *)(&meshExtension->assignment[0]),
		&bytes[*offset],
		faceCount * sizeof(uint16_t));
	*offset += faceCount * sizeof(uint16_t);

	meshExtension->textureName.resize(materialCount);
	meshExtension->maskName.resize(materialCount);
	char buffer[0x20];
	for (uint32_t i = 0; i < materialCount; i++) {
		// rw.read(buffer, 0x20);
		memcpy(buffer, &bytes[*offset], 0x20);
		*offset += 0x20;

		meshExtension->textureName[i] = buffer;
	}
	for (uint32_t i = 0; i < materialCount; i++) {
		// rw.read(buffer, 0x20);
		memcpy(buffer, &bytes[*offset], 0x20);
		*offset += 0x20;

		meshExtension->maskName[i] = buffer;
	}
	for (uint32_t i = 0; i < materialCount; i++) {
		meshExtension->unknowns.push_back(readFloat32(bytes, offset));
		meshExtension->unknowns.push_back(readFloat32(bytes, offset));
		meshExtension->unknowns.push_back(readFloat32(bytes, offset));
	}
}

bool Geometry::isDegenerateFace(uint32_t i, uint32_t j, uint32_t k)
{
	if (vertices[i*3+0] == vertices[j*3+0] &&
	    vertices[i*3+1] == vertices[j*3+1] &&
	    vertices[i*3+2] == vertices[j*3+2])
		return true;
	if (vertices[i*3+0] == vertices[k*3+0] &&
	    vertices[i*3+1] == vertices[k*3+1] &&
	    vertices[i*3+2] == vertices[k*3+2])
		return true;
	if (vertices[j*3+0] == vertices[k*3+0] &&
	    vertices[j*3+1] == vertices[k*3+1] &&
	    vertices[j*3+2] == vertices[k*3+2])
		return true;
	return false;
}

// native console data doesn't have face information, use this to generate it
void Geometry::generateFaces(void)
{
	faces.clear();

	for (uint32_t i = 0; i < splits.size(); i++) {
		Split &s = splits[i];
		if (faceType == FACETYPE_STRIP)
			for (uint32_t j = 0; j < s.indices.size()-2; j++) {
//				if (isDegenerateFace(s.indices[j+0],
//				    s.indices[j+1], s.indices[j+2]))
//					continue;
				if(s.indices[j+0] == s.indices[j+1] ||
				   s.indices[j+0] == s.indices[j+2] ||
				   s.indices[j+1] == s.indices[j+2])
					continue;
				faces.push_back(s.indices[j+1 + (j%2)]);
				faces.push_back(s.indices[j+0]);
				faces.push_back(s.matIndex);
				faces.push_back(s.indices[j+2 - (j%2)]);
			}
		else
			for (uint32_t j = 0; j < s.indices.size()-2; j+=3) {
				faces.push_back(s.indices[j+1]);
				faces.push_back(s.indices[j+0]);
				faces.push_back(s.matIndex);
				faces.push_back(s.indices[j+2]);
			}
	}
}

// these hold the (temporary) cleaned up data
vector<float> vertices_new;
vector<float> normals_new;
vector<float> texCoords_new[8];
vector<uint8_t> vertexColors_new;
vector<uint8_t> nightColors_new;
vector<uint32_t> vertexBoneIndices_new;
vector<float> vertexBoneWeights_new;

// used only by Geometry::cleanUp()
// adds new temporary vertex if it isn't already in the list
// and returns the new index of that vertex
uint32_t Geometry::addTempVertexIfNew(uint32_t index)
{
	// return if we already have the vertex
	for (uint32_t i = 0; i < vertices_new.size()/3; i++) {
		if (vertices_new[i*3+0] != vertices[index*3+0] ||
		    vertices_new[i*3+1] != vertices[index*3+1] ||
		    vertices_new[i*3+2] != vertices[index*3+2])
			continue;
		if (flags & FLAGS_NORMALS) {
			if (normals_new[i*3+0] != normals[index*3+0] ||
			    normals_new[i*3+1] != normals[index*3+1] ||
			    normals_new[i*3+2] != normals[index*3+2])
				continue;
		}
		if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2) {
			for (uint32_t j = 0; j < numUVs; j++)
				if (texCoords_new[j][i*2+0] !=
				                    texCoords[j][index*2+0] ||
				    texCoords_new[j][i*2+1] !=
				                    texCoords[j][index*2+1])
					goto cont;
		}
		if (flags & FLAGS_PRELIT) {
			if (vertexColors_new[i*4+0]!=vertexColors[index*4+0] ||
			    vertexColors_new[i*4+1]!=vertexColors[index*4+1] ||
			    vertexColors_new[i*4+2]!=vertexColors[index*4+2] ||
			    vertexColors_new[i*4+3]!=vertexColors[index*4+3])
				continue;
		}
		if (hasNightColors) {
			if (nightColors_new[i*4+0] != nightColors[index*4+0] ||
			    nightColors_new[i*4+1] != nightColors[index*4+1] ||
			    nightColors_new[i*4+2] != nightColors[index*4+2] ||
			    nightColors_new[i*4+3] != nightColors[index*4+3])
				continue;
		}
		if (hasSkin) {
			if (vertexBoneIndices_new[i]!=vertexBoneIndices[index])
				continue;

			if (vertexBoneWeights_new[i*4+0] !=
			                      vertexBoneWeights[index*4+0] ||
			    vertexBoneWeights_new[i*4+1] !=
			                      vertexBoneWeights[index*4+1] ||
			    vertexBoneWeights_new[i*4+2] !=
			                      vertexBoneWeights[index*4+2] ||
			    vertexBoneWeights_new[i*4+3] !=
			                      vertexBoneWeights[index*4+3])
				continue;
		}
		// this is only reached when the vertex is already in the list
		return i;

		cont: ;
	}

	// else add the vertex
	vertices_new.push_back(vertices[index*3+0]);
	vertices_new.push_back(vertices[index*3+1]);
	vertices_new.push_back(vertices[index*3+2]);
	if (flags & FLAGS_NORMALS) {
		normals_new.push_back(normals[index*3+0]);
		normals_new.push_back(normals[index*3+1]);
		normals_new.push_back(normals[index*3+2]);
	}
	if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2) {
		for (uint32_t j = 0; j < numUVs; j++) {
			texCoords_new[j].push_back(texCoords[j][index*2+0]);
			texCoords_new[j].push_back(texCoords[j][index*2+1]);
		}
	}
	if (flags & FLAGS_PRELIT) {
		vertexColors_new.push_back(vertexColors[index*4+0]);
		vertexColors_new.push_back(vertexColors[index*4+1]);
		vertexColors_new.push_back(vertexColors[index*4+2]);
		vertexColors_new.push_back(vertexColors[index*4+3]);
	}
	if (hasNightColors) {
		nightColors_new.push_back(nightColors[index*4+0]);
		nightColors_new.push_back(nightColors[index*4+1]);
		nightColors_new.push_back(nightColors[index*4+2]);
		nightColors_new.push_back(nightColors[index*4+3]);
	}
	if (hasSkin) {
		vertexBoneIndices_new.push_back(vertexBoneIndices[index]);

		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+0]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+1]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+2]);
		vertexBoneWeights_new.push_back(vertexBoneWeights[index*4+3]);
	}
	return vertices_new.size()/3 - 1;
}

// removes duplicate vertices (only useful with ps2 meshes)
void Geometry::cleanUp(void)
{
	vertices_new.clear();
	normals_new.clear();
	vertexColors_new.clear();
	nightColors_new.clear();
	for (uint32_t i = 0; i < 8; i++)
		texCoords_new[i].clear();
	vertexBoneIndices_new.clear();
	vertexBoneWeights_new.clear();

	vector<uint32_t> newIndices;

	// create new vertex list
	for (uint32_t i = 0; i < vertices.size()/3; i++)
		newIndices.push_back(addTempVertexIfNew(i));

	vertices = vertices_new;
	if (flags & FLAGS_NORMALS)
		normals = normals_new;
	if (flags & FLAGS_TEXTURED || flags & FLAGS_TEXTURED2)
		for (uint32_t j = 0; j < numUVs; j++)
			texCoords[j] = texCoords_new[j];
	if (flags & FLAGS_PRELIT)
		vertexColors = vertexColors_new;
	if (hasNightColors)
		nightColors = nightColors_new;
	if (hasSkin) {
		vertexBoneIndices = vertexBoneIndices_new;
		vertexBoneWeights = vertexBoneWeights_new;
	}

	// correct indices
	for (uint32_t i = 0; i < splits.size(); i++)
		for (uint32_t j = 0; j < splits[i].indices.size(); j++)
			splits[i].indices[j] = newIndices[splits[i].indices[j]];

	for (uint32_t i = 0; i < faces.size()/4; i++) {
		faces[i*4+0] = newIndices[faces[i*4+0]];
		faces[i*4+1] = newIndices[faces[i*4+1]];
		faces[i*4+3] = newIndices[faces[i*4+3]];
	}
}

void Geometry::dump(uint32_t index, string ind, bool detailed)
{
	cout << ind << "Geometry " << index << " {\n";
	ind += "  ";

	cout << ind << "flags: " << hex << flags << endl;
	cout << ind << "numUVs: " << dec << numUVs << endl;
	cout << ind << "hasNativeGeometry: " << hasNativeGeometry << endl;
	cout << ind << "triangleCount: " << faces.size()/4 << endl;
	cout << ind << "vertexCount: " << vertexCount << endl << endl;;

	if (flags & FLAGS_PRELIT) {
		cout << ind << "vertexColors {\n";
		ind += "  ";
		if (!detailed)
			cout << ind << "skipping\n";
		else
			for (uint32_t i = 0; i < vertexColors.size()/4; i++)
				cout << ind << int(vertexColors[i*4+0])<< ", "
					<< int(vertexColors[i*4+1]) << ", "
					<< int(vertexColors[i*4+2]) << ", "
					<< int(vertexColors[i*4+3]) << endl;
		ind = ind.substr(0, ind.size()-2);
		cout << ind << "}\n\n";
	}

	if (flags & FLAGS_TEXTURED) {
		cout << ind << "texCoords {\n";
		ind += "  ";
		if (!detailed)
			cout << ind << "skipping\n";
		else
			for (uint32_t i = 0; i < texCoords[0].size()/2; i++)
				cout << ind << texCoords[0][i*2+0]<< ", "
					<< texCoords[0][i*2+1] << endl;
		ind = ind.substr(0, ind.size()-2);
		cout << ind << "}\n\n";
	}
	if (flags & FLAGS_TEXTURED2) {
		for (uint32_t j = 0; j < numUVs; j++) {
		cout << ind << "texCoords " << j << " {\n";
		ind += "  ";
		if (!detailed)
			cout << ind << "skipping\n";
		else
			for (uint32_t i = 0; i < texCoords[j].size()/2; i++)
				cout << ind << texCoords[j][i*2+0]<< ", "
					<< texCoords[j][i*2+1] << endl;
		ind = ind.substr(0, ind.size()-2);
		cout << ind << "}\n\n";
		}
	}

	cout << ind << "faces {\n";
	ind += "  ";
	if (!detailed)
		cout << ind << "skipping\n";
	else
		for (uint32_t i = 0; i < faces.size()/4; i++)
			cout << ind << faces[i*4+0] << ", "
			            << faces[i*4+1] << ", "
			            << faces[i*4+2] << ", "
			            << faces[i*4+3] << endl;
	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n\n";

	cout << ind << "boundingSphere: ";
	for (uint32_t i = 0; i < 4; i++)
		cout << boundingSphere[i] << " ";
	cout << endl << endl;
	cout << ind << "hasPositions: " << hasPositions << endl;
	cout << ind << "hasNormals: " << hasNormals << endl;

	cout << ind << "vertices {\n";
	ind += "  ";
	if (!detailed)
		cout << ind << "skipping\n";
	else
		for (uint32_t i = 0; i < vertices.size()/3; i++)
			cout << ind << vertices[i*3+0] << ", "
			            << vertices[i*3+1] << ", "
			            << vertices[i*3+2] << endl;
	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";

	if (flags & FLAGS_NORMALS) {
		cout << ind << "normals {\n";
		ind += "  ";
		if (!detailed)
			cout << ind << "skipping\n";
		else
			for (uint32_t i = 0; i < normals.size()/3; i++)
				cout << ind << normals[i*3+0] << ", "
					    << normals[i*3+1] << ", "
					    << normals[i*3+2] << endl;
		ind = ind.substr(0, ind.size()-2);
		cout << ind << "}\n";
	}

	cout << endl << ind << "BinMesh {\n";
	ind += "  ";
	cout << ind << "faceType: " << faceType << endl;
	cout << ind << "numIndices: " << numIndices << endl;
	for (uint32_t i = 0; i < splits.size(); i++) {
		cout << endl << ind << "Split " << i << " {\n";
		ind += "  ";
		cout << ind << "matIndex: " << splits[i].matIndex << endl;
		cout << ind << "numIndices: "<<splits[i].indices.size() << endl;
		cout << ind << "indices {\n";
		if(!detailed)
			cout << ind+"  skipping\n";
		else
			for(uint32_t j = 0; j < splits[i].indices.size(); j++)
				cout << ind+" " << splits[i].indices[j] << endl;
		cout << ind << "}\n";
		ind = ind.substr(0, ind.size()-2);
		cout << ind << "}\n";
	}
	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";


	cout << endl << ind << "MaterialList {\n";
	ind += "  ";
	cout << ind << "numMaterials: " << materialList.size() << endl;
	for (uint32_t i = 0; i < materialList.size(); i++)
		materialList[i].dump(i, ind);
	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

Geometry::Geometry(void)
: flags(0), numUVs(0), hasNativeGeometry(false), vertexCount(0),
  hasNormals(false), faceType(0), numIndices(0), hasSkin(false), boneCount(0),
  specialIndexCount(0), unknown1(0), unknown2(0), hasMeshExtension(false),
  meshExtension(0), hasNightColors(false), nightColorsUnknown(0),
  has2dfx(false), hasMorph(false)
{
}

Geometry::Geometry(const Geometry &orig)
: flags(orig.flags), numUVs(orig.numUVs),
  hasNativeGeometry(orig.hasNativeGeometry), vertexCount(orig.vertexCount),
  faces(orig.faces), vertexColors(orig.vertexColors),
  hasPositions(orig.hasPositions), hasNormals(orig.hasNormals),
  vertices(orig.vertices), normals(orig.normals),
  materialList(orig.materialList), faceType(orig.faceType),
  numIndices(orig.numIndices), splits(orig.splits), hasSkin(orig.hasSkin),
  boneCount(orig.boneCount), specialIndexCount(orig.specialIndexCount),
  unknown1(orig.unknown1), unknown2(orig.unknown2),
  specialIndices(orig.specialIndices),
  vertexBoneIndices(orig.vertexBoneIndices),
  vertexBoneWeights(orig.vertexBoneWeights),
  inverseMatrices(orig.inverseMatrices),
  hasMeshExtension(orig.hasMeshExtension), hasNightColors(orig.hasNightColors),
  nightColorsUnknown(orig.nightColorsUnknown), nightColors(orig.nightColors),
  has2dfx(orig.has2dfx), hasMorph(orig.hasMorph)
{
	if (orig.meshExtension)
		meshExtension = new MeshExtension(*orig.meshExtension);
	else
		meshExtension = 0;

	for (uint32_t i = 0; i < 8; i++)
		texCoords[i] = orig.texCoords[i];
	for (uint32_t i = 0; i < 4; i++)
		boundingSphere[i] = orig.boundingSphere[i];
}

Geometry &Geometry::operator=(const Geometry &that)
{
	if (this != &that) {
		flags = that.flags;
		numUVs = that.numUVs;
		hasNativeGeometry = that.hasNativeGeometry;

		vertexCount = that.vertexCount;
		faces = that.faces;
		vertexColors = that.vertexColors;
		for (uint32_t i = 0; i < 8; i++)
			texCoords[i] = that.texCoords[i];

		for (uint32_t i = 0; i < 4; i++)
			boundingSphere[i] = that.boundingSphere[i];

		hasPositions = that.hasPositions;
		hasNormals = that.hasNormals;
		vertices = that.vertices;
		normals = that.normals;
		materialList = that.materialList;

		faceType = that.faceType;
		numIndices = that.numIndices;
		splits = that.splits;

		hasSkin = that.hasSkin;
		boneCount = that.boneCount;
		specialIndexCount = that.specialIndexCount;
		unknown1 = that.unknown1;
		unknown2 = that.unknown2;
		specialIndices = that.specialIndices;
		vertexBoneIndices = that.vertexBoneIndices;
		vertexBoneWeights = that.vertexBoneWeights;
		inverseMatrices = that.inverseMatrices;

		hasMeshExtension = that.hasMeshExtension;
		delete meshExtension;
		meshExtension = 0;
		if (that.meshExtension)
			meshExtension = new MeshExtension(*that.meshExtension);

		hasNightColors = that.hasNightColors;
		nightColorsUnknown = that.nightColorsUnknown;
		nightColors = that.nightColors;

		has2dfx = that.has2dfx;

		hasMorph = that.hasMorph;
	}
	return *this;
}

Geometry::~Geometry(void)
{
	delete meshExtension;
}


/*
 * Material
 */

void Material::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	//READ_HEADER(CHUNK_MATERIAL);
	header.read(bytes, offset);

	//READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	flags = readUInt32(bytes, offset);

	//rw.read((char *) (color), 4*sizeof(uint8_t));
	memcpy((char *)(color), &bytes[*offset], 4 * sizeof(uint8_t));
	*offset += 4 * sizeof(uint8_t);

	unknown = readUInt32(bytes, offset);
	hasTex = readInt32(bytes, offset);


	//rw.read((char *) (surfaceProps), 3*sizeof(float));
	memcpy((char *)(surfaceProps), &bytes[*offset], 3 * sizeof(float));
	*offset += 3 * sizeof(float);

	if (hasTex)
		texture.read(bytes, offset);

	readExtension(bytes, offset);
}

void Material::readExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;
	char buf[32];

	//READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset + header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_RIGHTTORENDER:
			hasRightToRender = true;
			rightToRenderVal1 = readUInt32(bytes, offset);
			rightToRenderVal2 = readUInt32(bytes, offset);
//cout << filename << " matrights: " << hex << rightToRenderVal1 << " " << rightToRenderVal2 << endl;
			break;
		case CHUNK_MATERIALEFFECTS: {
			hasMatFx = true;
			matFx = new MatFx;
			matFx->type = readUInt32(bytes, offset);
			switch (matFx->type) {
			case MATFX_BUMPMAP: {
//cout << filename << " BUMPMAP\n";
				// rw.seekg(4, ios::cur); // also MATFX_BUMPMAP
				*offset += 4;

				matFx->bumpCoefficient = readFloat32(bytes, offset);

				matFx->hasTex1 = readUInt32(bytes, offset);
				if (matFx->hasTex1)
					matFx->tex1.read(bytes, offset);

				matFx->hasTex2 = readUInt32(bytes, offset);
				if (matFx->hasTex2)
					matFx->tex2.read(bytes, offset);

				//rw.seekg(4, ios::cur); // 0
				*offset += 4;

				break;
			} case MATFX_ENVMAP: {
				// rw.seekg(4, ios::cur); // also MATFX_ENVMAP
				*offset += 4;

				matFx->envCoefficient = readFloat32(bytes, offset);

				matFx->hasTex1 = readUInt32(bytes, offset);
				if (matFx->hasTex1)
					matFx->tex1.read(bytes, offset);

				matFx->hasTex2 = readUInt32(bytes, offset);
				if (matFx->hasTex2)
					matFx->tex2.read(bytes, offset);

				//rw.seekg(4, ios::cur); // 0
				*offset += 4;

				break;
			} case MATFX_BUMPENVMAP: {
//cout << filename << " BUMPENVMAP\n";
				// rw.seekg(4, ios::cur); // MATFX_BUMPMAP
				*offset += 4;

				matFx->bumpCoefficient = readFloat32(bytes, offset);
				matFx->hasTex1 = readUInt32(bytes, offset);
				if (matFx->hasTex1)
					matFx->tex1.read(bytes, offset);
				// needs to be 0, tex2 will be used
				// rw.seekg(4, ios::cur);
				*offset += 4;

				// rw.seekg(4, ios::cur); // MATFX_ENVMPMAP
				*offset += 4;

				matFx->envCoefficient = readFloat32(bytes, offset);
				// needs to be 0, tex1 is already used
				// rw.seekg(4, ios::cur);
				*offset += 4;

				matFx->hasTex2 = readUInt32(bytes, offset);
				if (matFx->hasTex2)
					matFx->tex2.read(bytes, offset);
				break;
			} case MATFX_DUAL: {
//cout << filename << " DUAL\n";
				//rw.seekg(4, ios::cur); // also MATFX_DUAL
				*offset += 4;
				matFx->srcBlend = readUInt32(bytes, offset);
				matFx->destBlend = readUInt32(bytes, offset);

				matFx->hasDualPassMap = readUInt32(bytes, offset);
				if (matFx->hasDualPassMap)
					matFx->dualPassMap.read(bytes, offset);
				//rw.seekg(4, ios::cur); // 0
				*offset += 4;
				break;
			} case MATFX_UVTRANSFORM: {
//cout << filename << " UVTRANSFORM\n";
				// rw.seekg(4, ios::cur);//also MATFX_UVTRANSFORM
				*offset += 4;
				// rw.seekg(4, ios::cur); // 0
				*offset += 4;
				break;
			} case MATFX_DUALUVTRANSFORM: {
//cout << filename << " DUALUVTRANSFORM\n";
				// never observed in gta
				break;
			} default:
				break;
			}
			break;
		} case CHUNK_REFLECTIONMAT:
			hasReflectionMat = true;
			reflectionChannelAmount[0] = readFloat32(bytes, offset);
			reflectionChannelAmount[1] = readFloat32(bytes, offset);
			reflectionChannelAmount[2] = readFloat32(bytes, offset);
			reflectionChannelAmount[3] = readFloat32(bytes, offset);
			reflectionIntensity = readFloat32(bytes, offset);
			//rw.seekg(4, ios::cur);
			*offset += 4;
			break;
		case CHUNK_SPECULARMAT: {
			hasSpecularMat = true;
			specularLevel = readFloat32(bytes, offset);
			uint32_t len = header.length - sizeof(float) - 4;
			char *name = new char[len];
			//rw.read(name, len);
			memcpy(name, &bytes[*offset], len);
			*offset += len;

			specularName = name;
			//rw.seekg(4, ios::cur);
			*offset += 4;
			delete[] name;
			break;
		}
		case CHUNK_UVANIMPLG:
			//READ_HEADER(CHUNK_STRUCT);
			header.read(bytes, offset);
			hasUVAnim = true;
			uvVal = readUInt32(bytes, offset);
			//rw.read(buf, 32);
			memcpy(buf, &bytes[*offset], 32);
			*offset += 32;

			uvName = buf;
			break;
		default:
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		}
	}
}

void Material::dump(uint32_t index, string ind)
{
	cout << ind << "Material " << index << " {\n";
	ind += "  ";

	// unused
//	cout << ind << "flags: " << hex << flags << endl;
	cout << ind << "color: " << dec << int(color[0]) << " "
	                         << int(color[1]) << " "
	                         << int(color[2]) << " "
	                         << int(color[3]) << endl;
	// unused
//	cout << ind << "unknown: " << hex << unknown << endl;
	cout << ind << "surfaceProps: " << surfaceProps[0] << " "
	                                << surfaceProps[1] << " "
	                                << surfaceProps[2] << endl;

	if(hasTex)
		texture.dump(ind);

	if(hasMatFx)
		matFx->dump(ind);

	if(hasRightToRender){
		cout << hex;
		cout << ind << "Right to Render {\n";
		cout << ind+"  " << "val1: " << rightToRenderVal1<<endl;
		cout << ind+"  " << "val2: " << rightToRenderVal2<<endl;
		cout << ind << "}\n";
		cout << dec;
	}

	if(hasReflectionMat){
		cout << ind << "Reflection Material {\n";
		cout << ind+"  " << "amount: "
		     << reflectionChannelAmount[0] << " "
		     << reflectionChannelAmount[1] << " "
		     << reflectionChannelAmount[2] << " "
		     << reflectionChannelAmount[3] << endl;
		cout << ind+"  " << "intensity: " << reflectionIntensity << endl;
		cout << ind << "}\n";
	}

	if(hasSpecularMat){
		cout << ind << "Specular Material {\n";
		cout << ind+"  " << "level: " << specularLevel << endl;
		cout << ind+"  " << "name: " << specularName << endl;
		cout << ind << "}\n";
	}

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

Material::Material(void)
: flags(0), unknown(0), hasTex(false), hasRightToRender(false),
  rightToRenderVal1(0), rightToRenderVal2(0), hasMatFx(false), matFx(0),
  hasReflectionMat(false), reflectionIntensity(0.0f), hasSpecularMat(false),
  specularLevel(0.0f), hasUVAnim(false)
{
	for (int i = 0; i < 4; i++)
		color[i] = 0;
	for (int i = 0; i < 3; i++) {
		surfaceProps[i] = 0.0f;
		reflectionChannelAmount[i] = 0.0f;
	}
}

Material::Material(const Material& orig)
: flags(orig.flags), unknown(orig.unknown),
  hasTex(orig.hasTex), texture(orig.texture),
  hasRightToRender(orig.hasRightToRender),
  rightToRenderVal1(orig.rightToRenderVal1),
  rightToRenderVal2(orig.rightToRenderVal2),
  hasMatFx(orig.hasMatFx), hasReflectionMat(orig.hasReflectionMat),
  reflectionIntensity(orig.reflectionIntensity),
  hasSpecularMat(orig.hasSpecularMat), specularLevel(orig.specularLevel),
  specularName(orig.specularName), hasUVAnim(orig.hasUVAnim)
{
	if (orig.matFx)
		matFx = new MatFx(*orig.matFx);
	else
		matFx = 0;

	for (uint32_t i = 0; i < 4; i++)
		color[i] = orig.color[i];
	for (uint32_t i = 0; i < 3; i++)
		surfaceProps[i] = orig.surfaceProps[i];
	for (uint32_t i = 0; i < 4; i++)
		reflectionChannelAmount[i] = orig.reflectionChannelAmount[i];
}

Material &Material::operator=(const Material &that)
{
	if (this != &that) {
		flags = that.flags;
		for (uint32_t i = 0; i < 4; i++)
			color[i] = that.color[i];
		unknown = that.unknown;
		hasTex = that.hasTex;
		for (uint32_t i = 0; i < 3; i++)
			surfaceProps[i] = that.surfaceProps[i];

		texture = that.texture;

		hasRightToRender = that.hasRightToRender;
		rightToRenderVal1 = that.rightToRenderVal1;
		rightToRenderVal2 = that.rightToRenderVal2;

		hasMatFx = that.hasMatFx;
		delete matFx;
		matFx = 0;
		if (that.matFx)
			matFx = new MatFx(*that.matFx);

		hasReflectionMat = that.hasReflectionMat;
		for (uint32_t i = 0; i < 4; i++)
			reflectionChannelAmount[i] =
					that.reflectionChannelAmount[i];
		reflectionIntensity = that.reflectionIntensity;

		hasSpecularMat = that.hasSpecularMat;
		specularLevel = that.specularLevel;
		specularName = that.specularName;

		hasUVAnim = that.hasUVAnim;
	}
	return *this;
}

Material::~Material(void)
{
	delete matFx;
}

void MatFx::dump(string ind)
{
	static const char *names[] = {
		"INVALID",
		"MATFX_BUMPMAP",
		"MATFX_ENVMAP",
		"MATFX_BUMPENVMAP",
		"MATFX_DUAL",
		"MATFX_UVTRANSFORM",
		"MATFX_DUALUVTRANSFORM"
	};
	cout << ind << "MatFX {\n";
	ind += "  ";
	cout << ind << "type: " << names[type] << endl;
	if(type == MATFX_BUMPMAP || type == MATFX_BUMPENVMAP)
		cout << ind << "bumpCoefficient: " << bumpCoefficient << endl;
	if(type == MATFX_ENVMAP || type == MATFX_BUMPENVMAP)
		cout << ind << "envCoefficient: " << envCoefficient << endl;
	if(type == MATFX_DUAL){
		cout << ind << "srcBlend: " << srcBlend << endl;
		cout << ind << "destBlend: " << destBlend << endl;
	}
	cout << ind << "textures: " << hasTex1 << " " << hasTex2 << " " << hasDualPassMap << endl;
	if(hasTex1)
		tex1.dump(ind);
	if(hasTex2)
		tex2.dump(ind);
	if(hasDualPassMap)
		dualPassMap.dump(ind);

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

MatFx::MatFx(void)
: hasTex1(false), hasTex2(false), hasDualPassMap(false)
{
}

/*
 * Texture
 */

void Texture::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	// READ_HEADER(CHUNK_TEXTURE);
	header.read(bytes, offset);

	// READ_HEADER(CHUNK_STRUCT);
	header.read(bytes, offset);

	filterFlags = readUInt16(bytes, offset);
	//rw.seekg(2, ios::cur);
	*offset += 2;

	// READ_HEADER(CHUNK_STRING);
	header.read(bytes, offset);

	char *buffer = new char[header.length+1];
	//rw.read(buffer, header.length);
	memcpy(buffer, &bytes[*offset], header.length);
	*offset += header.length;

	buffer[header.length] = '\0';
	name = buffer;
	delete[] buffer;

	// READ_HEADER(CHUNK_STRING);
	header.read(bytes, offset);

	buffer = new char[header.length+1];
	//rw.read(buffer, header.length);
	memcpy(buffer, &bytes[*offset], header.length);
	*offset += header.length;

	buffer[header.length] = '\0';
	maskName = buffer;
	delete[] buffer;

	readExtension(bytes, offset);
}

void Texture::readExtension(char *bytes, size_t *offset)
{
	HeaderInfo header;
	// READ_HEADER(CHUNK_EXTENSION);
	header.read(bytes, offset);

	streampos end = *offset + header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_SKYMIPMAP:
			hasSkyMipmap = true;
			//rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		default:
			// rw.seekg(header.length, ios::cur);
			*offset += header.length;
			break;
		}
	}
}

void Texture::dump(std::string ind)
{
	cout << ind << "Texture {\n";
	ind += "  ";

	cout << ind << "filterFlags: " << hex << filterFlags << dec << endl;
	cout << ind << "name: " << name << endl;
	cout << ind << "maskName: " << maskName << endl;

	ind = ind.substr(0, ind.size()-2);
	cout << ind << "}\n";
}

Texture::Texture(void)
: filterFlags(0), name(""), maskName(""), hasSkyMipmap(false)
{
}
