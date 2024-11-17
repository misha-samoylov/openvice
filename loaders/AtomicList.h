#pragma once

#include <stdint.h>
#include "../renderware.h"

class Atomic
{
public:
	void Read(char* bytes, size_t* offset);
	void ReadExtension(char* bytes, size_t* offset);
	void Dump(uint32_t index);

	void Init();
	void Cleanup();

private:
	int32_t m_frameIndex;
	int32_t m_geometryIndex;

	/* Extensions */

	/* right to render */
	bool m_hasRightToRender;
	uint32_t m_rightToRenderVal1;
	uint32_t m_rightToRenderVal2;

	/* particles */
	bool m_hasParticles;
	uint32_t m_particlesVal;

	/* pipelineset */
	bool m_hasPipelineSet;
	uint32_t m_pipelineSetVal;

	/* material fx */
	bool m_hasMaterialFx;
	uint32_t m_materialFxVal;
};


class AtomicList
{
public:
	void Read(uint32_t numAtomics, char* bytes, size_t* offset);
	void Cleanup();
	Atomic* GetAtomic(int id);
	uint32_t GetNumAtomic();

private:
	Atomic** m_atomics;
	uint32_t m_numAtomics;
};
