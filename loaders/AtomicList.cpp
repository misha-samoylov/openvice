#include "AtomicList.h"

void Atomic::Read(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_ATOMIC */
	header.read(bytes, offset); /* CHUNK_STRUCT */

	m_frameIndex = readUInt32(bytes, offset);
	m_geometryIndex = readUInt32(bytes, offset);
	*offset += 8; /* constant */

	ReadExtension(bytes, offset);
}

void Atomic::ReadExtension(char* bytes, size_t* offset)
{
	HeaderInfo header;

	header.read(bytes, offset); /* CHUNK_EXTENSION */

	size_t end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);
		switch (header.type) {
		case CHUNK_RIGHTTORENDER:
			m_hasRightToRender = true;
			m_rightToRenderVal1 = readUInt32(bytes, offset);
			m_rightToRenderVal2 = readUInt32(bytes, offset);
			break;
		case CHUNK_PARTICLES:
			m_hasParticles = true;
			m_particlesVal = readUInt32(bytes, offset);
			break;
		case CHUNK_MATERIALEFFECTS:
			m_hasMaterialFx = true;
			m_materialFxVal = readUInt32(bytes, offset);
			break;
		case CHUNK_PIPELINESET:
			m_hasPipelineSet = true;
			m_pipelineSetVal = readUInt32(bytes, offset);
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

	printf("frameIndex: %d\n", m_frameIndex);
	printf("geometryIndex: %d\n", m_geometryIndex);

	if (m_hasRightToRender) {
		printf("Right to Render\n");
		printf("val1: %d\n", m_rightToRenderVal1);
		printf("val2: %d\n", m_rightToRenderVal2);
	}

	if (m_hasParticles)
		printf("particlesVal: %d\n", m_particlesVal);

	if (m_hasPipelineSet)
		printf("pipelineSetVal: %d\n", m_pipelineSetVal);

	if (m_hasMaterialFx)
		printf("materialFxVal: %d\n", m_materialFxVal);
}

void Atomic::Init()
{
	m_frameIndex = -1;
	m_geometryIndex = -1;

	m_hasRightToRender = false;
	m_rightToRenderVal1 = 0;
	m_rightToRenderVal2 = 0;

	m_hasParticles = false;
	m_particlesVal = 0;
	m_hasPipelineSet = false;
	m_pipelineSetVal = 0;

	m_hasMaterialFx = false;
	m_materialFxVal = 0;
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

Atomic* AtomicList::GetAtomic(int modelId)
{
	return m_atomics[modelId];
}

uint32_t AtomicList::GetNumAtomic()
{
	return m_numAtomics;
}
