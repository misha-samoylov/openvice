#pragma once

#include "../renderware.h"

class Frame
{
public:
	void Init();
	void Cleanup();

	void ReadStruct(char* bytes, size_t* offset);
	void ReadExtension(char* bytes, size_t* offset);
	void Dump(uint32_t index);

	int32_t GetParent() const { return m_parent; }
	const float* GetPosition() const { return m_position; }
	const float* GetRotationMatrix() const { return m_rotationMatrix; }
	const char* GetName() const { return m_name; }
	bool HasHAnim() const { return m_hasHAnim; }
	int32_t GetHAnimBoneId() const { return m_hAnimBoneId; }
	uint32_t GetAnimBoneCount() const { return m_AnimBoneCount; }
	const int32_t* GetHAnimBoneIds() const { return m_hAnimBoneIds; }
	const uint32_t* GetHAnimBoneTypes() const { return m_hAnimBoneTypes; }

private:
	float m_rotationMatrix[9];
	float m_position[3];
	int32_t m_parent;

	/* Extensions */
	char *m_name; /* Node name */
	bool m_hasHAnim;
	uint32_t m_hAnimUnknown1;
	int32_t m_hAnimBoneId;
	uint32_t m_AnimBoneCount;
	uint32_t m_hAnimUnknown2;
	uint32_t m_hAnimUnknown3;
	int32_t *m_hAnimBoneIds; /* Array */
	uint32_t *m_hAnimBoneNumbers; /* Array */
	uint32_t *m_hAnimBoneTypes; /* Array */
};

class FrameList
{
public:
	void Read(char* bytes, size_t* offset);
	void Cleanup();
	int GetNumFrames();
	Frame* GetFrame(int modelId);

private:
	int m_numFrames;
	Frame** m_frames; /* Array of classes */
};
