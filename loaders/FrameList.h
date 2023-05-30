#pragma once

#include "../renderware.h"

class Frame
{
public:
	float rotationMatrix[9];
	float position[3];
	int32_t parent;

	/* Extensions */

	/* node name */
	char *m_name;

	/* hanim */
	bool hasHAnim;
	uint32_t hAnimUnknown1;
	int32_t hAnimBoneId;
	uint32_t hAnimBoneCount;
	uint32_t hAnimUnknown2;
	uint32_t hAnimUnknown3;
	int32_t *hAnimBoneIds; // Array
	uint32_t *hAnimBoneNumbers; // Array
	uint32_t *hAnimBoneTypes; // Array

	Frame();
	~Frame();

	void readStruct(char* bytes, size_t* offset);
	void readExtension(char* bytes, size_t* offset);

	void dump(uint32_t index, std::string ind = "");
};

class FrameList
{
public:
	int m_numFrames;
	Frame** m_frames;

	void read(char* bytes, size_t* offset);
	~FrameList();
};
