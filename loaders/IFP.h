#pragma once

#include <stdint.h>
#include <vector>
#include <string>

struct IfpKeyFrame {
	float rx, ry, rz, rw;
	float tx, ty, tz;
	float time; /* absolute until CalcTotalTime, then delta */
	bool hasTranslation;
};

struct IfpSequence {
	char name[24];
	int32_t boneTag;
	std::vector<IfpKeyFrame> frames;
};

struct IfpAnim {
	char name[24];
	float totalLength;
	std::vector<IfpSequence> sequences;
};

class IFP
{
public:
	bool Load(const char* path);
	void Cleanup();

	IfpAnim* FindAnim(const char* name);
	int GetAnimCount() const { return (int)m_anims.size(); }

private:
	std::vector<IfpAnim> m_anims;
	char m_blockName[24];

	static void RoundSize(uint32_t& size);
	void CalcTotalTime(IfpAnim& anim);
	void RemoveQuaternionFlips(IfpAnim& anim);
};
