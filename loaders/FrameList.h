#pragma once

#include "../renderware.h"

struct Frame
{
	float m_rotationMatrix[9];
	float m_position[3];
	int32_t m_parent;

	/* Extensions */

	/* node name */
	char *m_name;

	/* hanim */
	bool m_hasHAnim;
	uint32_t m_hAnimUnknown1;
	int32_t m_hAnimBoneId;
	uint32_t m_AnimBoneCount;
	uint32_t m_hAnimUnknown2;
	uint32_t m_hAnimUnknown3;
	int32_t *m_hAnimBoneIds; // Array
	uint32_t *m_hAnimBoneNumbers; // Array
	uint32_t *m_hAnimBoneTypes; // Array

	void Init();
	void Cleanup();

	void ReadStruct(char* bytes, size_t* offset);
	void ReadExtension(char* bytes, size_t* offset);
	void Dump(uint32_t index);
};

class FrameList
{
public:
	int m_numFrames;
	Frame** m_frames; // Array of classes

	void Read(char* bytes, size_t* offset);
	void Cleanup();
};
