#include "FrameList.h"

// Only reads part of the frame struct
void Frame::readStruct(char* bytes, size_t* offset)
{
	memcpy((char*)rotationMatrix, &bytes[*offset], 9 * sizeof(float));
	*offset += 9 * sizeof(float);

	memcpy((char*)position, &bytes[*offset], 3 * sizeof(float));
	*offset += 3 * sizeof(float);

	parent = readInt32(bytes, offset);

	*offset += 4; // Matrix creation flag, unused
}

void Frame::readExtension(char* bytes, size_t* offset)
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
			hasHAnim = true;

			hAnimUnknown1 = readUInt32(bytes, offset);
			hAnimBoneId = readInt32(bytes, offset);;
			hAnimBoneCount = readUInt32(bytes, offset);

			if (hAnimBoneCount != 0) {
				hAnimUnknown2 = readUInt32(bytes, offset);;
				hAnimUnknown3 = readUInt32(bytes, offset);;
			}

			hAnimBoneIds = (int32_t*)malloc(hAnimBoneCount * sizeof(int32_t));
			hAnimBoneNumbers = (uint32_t*)malloc(hAnimBoneCount * sizeof(uint32_t));
			hAnimBoneTypes = (uint32_t*)malloc(hAnimBoneCount * sizeof(uint32_t));

			for (uint32_t i = 0; i < hAnimBoneCount; i++) {
				hAnimBoneIds[i] = readInt32(bytes, offset);
				hAnimBoneNumbers[i] = readUInt32(bytes, offset);

				uint32_t flag = readUInt32(bytes, offset);

				if ((flag & ~0x3) != 0)
					cout << flag << endl;

				hAnimBoneTypes[i] = flag;
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

void Frame::dump(uint32_t index, string ind)
{
	cout << ind << "Frame " << index << " {\n";
	ind += "  ";

	cout << ind << "rotationMatrix: ";
	for (uint32_t i = 0; i < 9; i++)
		cout << rotationMatrix[i] << " ";
	cout << endl;

	cout << ind << "position: ";
	for (uint32_t i = 0; i < 3; i++)
		cout << position[i] << " ";
	cout << endl;
	cout << ind << "parent: " << parent << endl;

	cout << ind << "name: " << m_name << endl;

	// TODO: HANIM

	ind = ind.substr(0, ind.size() - 2);
	cout << ind << "}\n";
}

Frame::Frame()
{
	parent = -1;
	hasHAnim = false;
	hAnimUnknown1 = 0;
	hAnimBoneId = -1;
	hAnimBoneCount = 0;
	hAnimUnknown2 = 0;
	hAnimUnknown3 = 0;

	m_name = NULL;
	hAnimBoneIds = NULL;
	hAnimBoneNumbers = NULL;
	hAnimBoneTypes = NULL;

	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;

		for (int j = 0; j < 3; j++) {
			rotationMatrix[i * 3 + j] = (i == j) ? 1.0f : 0.0f;
		}
	}
}

Frame::~Frame()
{
	free(hAnimBoneIds);
	free(hAnimBoneNumbers);
	free(hAnimBoneTypes);
}

void FrameList::read(char *bytes, size_t *offset)
{
	HeaderInfo header;

	header.read(bytes, offset); // CHUNK_FRAMELIST
	header.read(bytes, offset); // CHUNK_STRUCT

	uint32_t m_numFrames = readUInt32(bytes, offset);

	Frame **m_frames = new Frame * [m_numFrames];
	
	for (uint32_t i = 0; i < m_numFrames; i++) { // Init dynamic array of classes
		m_frames[i] = new Frame();
	}

	for (uint32_t i = 0; i < m_numFrames; i++) {
		m_frames[i]->readStruct(bytes, offset);
	}

	for (uint32_t i = 0; i < m_numFrames; i++) {
		m_frames[i]->readExtension(bytes, offset);
	}
}

FrameList::~FrameList()
{
	for (int i = 0; i < m_numFrames; ++i) {
		delete[] m_frames[i];
	}

	delete[] m_frames;
}