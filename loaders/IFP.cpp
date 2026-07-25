#include "IFP.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>

void IFP::RoundSize(uint32_t& size)
{
	if (size & 3)
		size += 4 - (size & 3);
}

bool IFP::Load(const char* path)
{
	Cleanup();

	FILE* f = fopen(path, "rb");
	if (!f) {
		printf("[Error] Cannot open IFP %s\n", path);
		return false;
	}

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);

	std::vector<char> data((size_t)fileSize);
	if (fread(data.data(), 1, (size_t)fileSize, f) != (size_t)fileSize) {
		fclose(f);
		printf("[Error] Failed reading IFP %s\n", path);
		return false;
	}
	fclose(f);

	const char* p = data.data();
	const char* end = p + fileSize;

	auto readHeader = [&](char ident[4], uint32_t& size) -> bool {
		if (p + 8 > end)
			return false;
		memcpy(ident, p, 4);
		memcpy(&size, p + 4, 4);
		p += 8;
		RoundSize(size);
		return true;
	};

	char ident[4];
	uint32_t size = 0;

	if (!readHeader(ident, size) || strncmp(ident, "ANPK", 4) != 0) {
		printf("[Error] IFP is not ANPK: %s\n", path);
		return false;
	}

	if (!readHeader(ident, size) || strncmp(ident, "INFO", 4) != 0) {
		printf("[Error] IFP missing INFO\n");
		return false;
	}

	if (p + size > end)
		return false;

	int32_t numAnims = 0;
	memcpy(&numAnims, p, 4);
	strncpy(m_blockName, p + 4, sizeof(m_blockName) - 1);
	m_blockName[sizeof(m_blockName) - 1] = '\0';
	p += size;

	printf("[Info] IFP block '%s' anims=%d\n", m_blockName, numAnims);

	m_anims.reserve(numAnims);

	for (int a = 0; a < numAnims; a++) {
		IfpAnim anim;
		memset(anim.name, 0, sizeof(anim.name));
		anim.totalLength = 0.0f;

		if (!readHeader(ident, size) || strncmp(ident, "NAME", 4) != 0)
			return false;
		if (p + size > end)
			return false;
		strncpy(anim.name, p, sizeof(anim.name) - 1);
		p += size;

		if (!readHeader(ident, size) || strncmp(ident, "DGAN", 4) != 0)
			return false;

		if (!readHeader(ident, size) || strncmp(ident, "INFO", 4) != 0)
			return false;
		if (p + size > end)
			return false;

		int32_t numSequences = 0;
		memcpy(&numSequences, p, 4);
		p += size;

		anim.sequences.resize(numSequences);

		for (int s = 0; s < numSequences; s++) {
			IfpSequence& seq = anim.sequences[s];
			memset(seq.name, 0, sizeof(seq.name));
			seq.boneTag = -1;

			if (!readHeader(ident, size) || strncmp(ident, "CPAN", 4) != 0)
				return false;

			uint32_t animSize = 0;
			if (!readHeader(ident, animSize) || strncmp(ident, "ANIM", 4) != 0)
				return false;
			if (p + animSize > end)
				return false;

			strncpy(seq.name, p, sizeof(seq.name) - 1);
			int32_t numFrames = 0;
			memcpy(&numFrames, p + 28, 4);
			if (animSize == 44)
				memcpy(&seq.boneTag, p + 40, 4);
			p += animSize;

			if (numFrames <= 0)
				continue;

			if (!readHeader(ident, size))
				return false;

			bool hasScale = false;
			bool hasTranslation = false;
			if (strncmp(ident, "KRTS", 4) == 0) {
				hasScale = true;
				hasTranslation = true;
			} else if (strncmp(ident, "KRT0", 4) == 0) {
				hasTranslation = true;
			} else if (strncmp(ident, "KR00", 4) != 0) {
				printf("[Error] Unknown keyframe type %.4s in %s\n", ident, anim.name);
				return false;
			}

			seq.frames.resize(numFrames);
			for (int k = 0; k < numFrames; k++) {
				IfpKeyFrame& kf = seq.frames[k];
				kf.hasTranslation = hasTranslation;
				kf.tx = kf.ty = kf.tz = 0.0f;

				if (hasScale) {
					if (p + 0x2C > end)
						return false;
					float buf[11];
					memcpy(buf, p, 0x2C);
					p += 0x2C;
					/* Invert quaternion (conjugate) like re3 */
					kf.rx = -buf[0];
					kf.ry = -buf[1];
					kf.rz = -buf[2];
					kf.rw = buf[3];
					kf.tx = buf[4];
					kf.ty = buf[5];
					kf.tz = buf[6];
					kf.time = buf[10];
				} else if (hasTranslation) {
					if (p + 0x20 > end)
						return false;
					float buf[8];
					memcpy(buf, p, 0x20);
					p += 0x20;
					kf.rx = -buf[0];
					kf.ry = -buf[1];
					kf.rz = -buf[2];
					kf.rw = buf[3];
					kf.tx = buf[4];
					kf.ty = buf[5];
					kf.tz = buf[6];
					kf.time = buf[7];
				} else {
					if (p + 0x14 > end)
						return false;
					float buf[5];
					memcpy(buf, p, 0x14);
					p += 0x14;
					kf.rx = -buf[0];
					kf.ry = -buf[1];
					kf.rz = -buf[2];
					kf.rw = buf[3];
					kf.time = buf[4];
				}
			}
		}

		RemoveQuaternionFlips(anim);
		CalcTotalTime(anim);
		m_anims.push_back(std::move(anim));
	}

	printf("[Info] Loaded %d animations from %s\n", (int)m_anims.size(), path);
	return true;
}

void IFP::RemoveQuaternionFlips(IfpAnim& anim)
{
	for (size_t s = 0; s < anim.sequences.size(); s++) {
		IfpSequence& seq = anim.sequences[s];
		if (seq.frames.size() < 2)
			continue;

		for (size_t i = 1; i < seq.frames.size(); i++) {
			IfpKeyFrame& prev = seq.frames[i - 1];
			IfpKeyFrame& cur = seq.frames[i];
			float dot = prev.rx * cur.rx + prev.ry * cur.ry + prev.rz * cur.rz + prev.rw * cur.rw;
			if (dot < 0.0f) {
				cur.rx = -cur.rx;
				cur.ry = -cur.ry;
				cur.rz = -cur.rz;
				cur.rw = -cur.rw;
			}
		}
	}
}

void IFP::CalcTotalTime(IfpAnim& anim)
{
	anim.totalLength = 0.0f;

	for (size_t s = 0; s < anim.sequences.size(); s++) {
		IfpSequence& seq = anim.sequences[s];
		if (seq.frames.empty())
			continue;

		float last = seq.frames.back().time;
		if (last > anim.totalLength)
			anim.totalLength = last;

		for (int j = (int)seq.frames.size() - 1; j >= 1; j--)
			seq.frames[j].time -= seq.frames[j - 1].time;
	}
}

IfpAnim* IFP::FindAnim(const char* name)
{
	for (size_t i = 0; i < m_anims.size(); i++) {
		if (_stricmp(m_anims[i].name, name) == 0)
			return &m_anims[i];
	}
	return nullptr;
}

void IFP::Cleanup()
{
	m_anims.clear();
	memset(m_blockName, 0, sizeof(m_blockName));
}
