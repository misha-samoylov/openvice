#include "AtomicList.h"

void Atomic::Read(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_ATOMIC */
	header.read(bytes, offset); /* CHUNK_STRUCT */

	frameIndex = readUInt32(bytes, offset);
	geometryIndex = readUInt32(bytes, offset);
	*offset += 8; /* constant */

	ReadExtension(bytes, offset);
}

void Atomic::ReadExtension(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_EXTENSION */

	streampos end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_RIGHTTORENDER:
			hasRightToRender = true;
			rightToRenderVal1 = readUInt32(bytes, offset);
			rightToRenderVal2 = readUInt32(bytes, offset);
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
			break;
		default:
			*offset += header.length;
			break;
		}
	}
}

void Atomic::Dump(uint32_t index)
{
	printf("Atomic %d\n", index);

	printf("frameIndex: %d\n", frameIndex);
	printf("geometryIndex: %d\n", geometryIndex);

	if (hasRightToRender) {
		printf("Right to Render\n");
		printf("val1: %d\n", rightToRenderVal1);
		printf("val2: %d\n", rightToRenderVal2);
	}

	if (hasParticles)
		printf("particlesVal: %d\n", particlesVal);

	if (hasPipelineSet)
		printf("pipelineSetVal: %d\n", pipelineSetVal);

	if (hasMaterialFx)
		printf("materialFxVal: %d\n", materialFxVal);
}

void Atomic::Init()
{
	frameIndex = -1;
	geometryIndex = -1;

	hasRightToRender = false;
	rightToRenderVal1 = 0;
	rightToRenderVal2 = 0;

	hasParticles = false;
	particlesVal = 0;
	hasPipelineSet = false;
	pipelineSetVal = 0;

	hasMaterialFx = false;
	materialFxVal = 0;
}

void Atomic::Cleanup()
{
	/* None to free */
}

void AtomicList::Read(uint32_t numAtomics, char* bytes, size_t* offset)
{
	m_atomics = new Atomic * [numAtomics];
	m_numAtomics = numAtomics;

	for (uint32_t i = 0; i < numAtomics; i++) {
		m_atomics[i] = new Atomic();
		m_atomics[i]->Init();
	}

	for (uint32_t i = 0; i < numAtomics; i++) {
		m_atomics[i]->Read(bytes, offset);
	}
}

void AtomicList::Cleanup()
{
	for (int i = 0; i < m_numAtomics; i++) {
		m_atomics[i]->Cleanup();
		delete m_atomics[i];
	}

	delete[] m_atomics;
}

Atomic* AtomicList::GetAtomic(int id)
{
	return m_atomics[id];
}
