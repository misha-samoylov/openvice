#pragma once

#include <stdint.h>
#include "../renderware.h"

class Atomic
{
private:
	int32_t frameIndex;
	int32_t geometryIndex;

	/* Extensions */

	/* right to render */
	bool hasRightToRender;
	uint32_t rightToRenderVal1;
	uint32_t rightToRenderVal2;

	/* particles */
	bool hasParticles;
	uint32_t particlesVal;

	/* pipelineset */
	bool hasPipelineSet;
	uint32_t pipelineSetVal;

	/* material fx */
	bool hasMaterialFx;
	uint32_t materialFxVal;
public:
	void Read(char* bytes, size_t* offset);
	void ReadExtension(char* bytes, size_t* offset);
	void Dump(uint32_t index);

	void Init();
	void Cleanup();
};


class AtomicList
{
public:
	void Read(uint32_t numAtomics, char* bytes, size_t* offset);
	void Cleanup();
	Atomic* GetAtomic(int id);

private:
	Atomic** m_atomics;
	uint32_t m_numAtomics;
};
