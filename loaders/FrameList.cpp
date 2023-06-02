#include "FrameList.h"

// Only reads part of the frame struct
void Frame::ReadStruct(char* bytes, size_t* offset)
{
	memcpy((char*)m_rotationMatrix, &bytes[*offset], 9 * sizeof(float));
	*offset += 9 * sizeof(float);

	memcpy((char*)m_position, &bytes[*offset], 3 * sizeof(float));
	*offset += 3 * sizeof(float);

	m_parent = readInt32(bytes, offset);

	*offset += 4; // Matrix creation flag, unused
}

void Frame::ReadExtension(char* bytes, size_t* offset)
{
	HeaderInfo header;
	header.read(bytes, offset); // CHUNK_EXTENSION

	streampos end = *offset;
	end += header.length;

	while (*offset < end) {
		header.read(bytes, offset);

		switch (header.type) {
		case CHUNK_FRAME:
		{
			m_name = (char*)malloc((header.length + 1) * sizeof(char));
			memcpy(m_name, &bytes[*offset], header.length);
			*offset += header.length;

			m_name[header.length] = '\0';
			break;
		}
		case CHUNK_HANIM:
			m_hasHAnim = true;

			m_hAnimUnknown1 = readUInt32(bytes, offset);
			m_hAnimBoneId = readInt32(bytes, offset);;
			m_AnimBoneCount = readUInt32(bytes, offset);

			if (m_AnimBoneCount != 0) {
				m_hAnimUnknown2 = readUInt32(bytes, offset);;
				m_hAnimUnknown3 = readUInt32(bytes, offset);;
			}

			if (m_AnimBoneCount > 0) {
				m_hAnimBoneIds = (int32_t*)malloc(m_AnimBoneCount * sizeof(int32_t));
				m_hAnimBoneNumbers = (uint32_t*)malloc(m_AnimBoneCount * sizeof(uint32_t));
				m_hAnimBoneTypes = (uint32_t*)malloc(m_AnimBoneCount * sizeof(uint32_t));

				for (uint32_t i = 0; i < m_AnimBoneCount; i++) {
					m_hAnimBoneIds[i] = readInt32(bytes, offset);
					m_hAnimBoneNumbers[i] = readUInt32(bytes, offset);

					uint32_t flag = readUInt32(bytes, offset);

					if ((flag & ~0x3) != 0)
						cout << flag << endl;

					m_hAnimBoneTypes[i] = flag;
				}
			}
			break;
		default:
			*offset += header.length;
			break;
		}
	}
	//	if(hasHAnim)
	//		cout << hAnimBoneId << " " << name << endl;
}

void Frame::Dump(uint32_t index)
{
	printf("Frame %d\n", index);
	printf("rotationMatrix:\n");

	for (uint32_t i = 0; i < 9; i++)
		printf("%f\n", m_rotationMatrix[i]);

	printf("position:\n");
	for (uint32_t i = 0; i < 3; i++)
		printf("%f\n", m_position[i]);
	
	printf("parent: %d\n", m_parent);
	printf("name: %s\n", m_name);
}

void Frame::Init()
{
	m_parent = -1;
	m_hasHAnim = false;
	m_hAnimUnknown1 = 0;
	m_hAnimBoneId = -1;
	m_AnimBoneCount = 0;
	m_hAnimUnknown2 = 0;
	m_hAnimUnknown3 = 0;

	m_name = NULL;
	m_hAnimBoneIds = NULL;
	m_hAnimBoneNumbers = NULL;
	m_hAnimBoneTypes = NULL;

	for (int i = 0; i < 3; i++) {
		m_position[i] = 0.0f;

		for (int j = 0; j < 3; j++) {
			m_rotationMatrix[i * 3 + j] = (i == j) ? 1.0f : 0.0f;
		}
	}
}

void Frame::Cleanup()
{
	free(m_hAnimBoneIds);
	free(m_hAnimBoneNumbers);
	free(m_hAnimBoneTypes);
}

void FrameList::Read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	header.read(bytes, offset); // CHUNK_FRAMELIST
	header.read(bytes, offset); // CHUNK_STRUCT

	m_numFrames = readUInt32(bytes, offset);

	m_frames = new Frame * [m_numFrames];

	for (uint32_t i = 0; i < m_numFrames; i++) {
		m_frames[i] = new Frame();
		m_frames[i]->Init();
	}

	for (uint32_t i = 0; i < m_numFrames; i++) {
		m_frames[i]->ReadStruct(bytes, offset);
	}

	for (uint32_t i = 0; i < m_numFrames; i++) {
		m_frames[i]->ReadExtension(bytes, offset);
	}
}

void FrameList::Cleanup()
{
	for (int i = 0; i < m_numFrames; i++) {
		m_frames[i]->Cleanup();
		delete m_frames[i];
	}

	delete[] m_frames;
}
