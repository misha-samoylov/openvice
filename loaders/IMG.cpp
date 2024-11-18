#include "IMG.hpp"

int IMG::OpenFileDir(TCHAR *filepath)
{
	HANDLE m_hFileDir = CreateFile(filepath, GENERIC_READ, 0, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (m_hFileDir == INVALID_HANDLE_VALUE) {
		printf("fileMappingCreate - CreateFile failed, fname = %s\n", filepath);
		return 1;
	}

	DWORD dwFileSize = ::GetFileSize(m_hFileDir, nullptr);
	if (dwFileSize == INVALID_FILE_SIZE) {
		printf("fileMappingCreate - GetFileSize failed, fname = %s\n", filepath);
		CloseHandle(m_hFileDir);
		return 1;
	}

	HANDLE m_hMappingDir = CreateFileMapping(m_hFileDir, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (m_hMappingDir == nullptr) {
		printf("fileMappingCreate - CreateFileMapping failed, fname = %s\n", filepath);
		CloseHandle(m_hFileDir);
		return 1;
	}

	m_dataPtrDir = (unsigned char*)MapViewOfFile(m_hMappingDir,
		FILE_MAP_READ,
		0,
		0,
		dwFileSize
	);
	if (m_dataPtrDir == nullptr) {
		printf("fileMappingCreate - MapViewOfFile failed, fname = %s\n", filepath);
		CloseHandle(m_hMappingDir);
		CloseHandle(m_hFileDir);
		return 1;
	}

	m_countFiles = dwFileSize / sizeof(struct DirEntry);
	m_pFilesDir = (struct DirEntry*)m_dataPtrDir;

    return 0;
}

void IMG::DumpFileDir()
{
	printf("[Dump] dir file[0].name = %s\n", m_pFilesDir[0].name);
}

int IMG::OpenFileImg(TCHAR* filepath)
{
	HANDLE m_hFileImg = CreateFile(filepath, GENERIC_READ, 0, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (m_hFileImg == INVALID_HANDLE_VALUE) {
		printf("fileMappingCreate - CreateFile failed, fname = %s\n", filepath);
		return 1;
	}

	DWORD dwFileSize = ::GetFileSize(m_hFileImg, nullptr);
	if (dwFileSize == INVALID_FILE_SIZE) {
		printf("fileMappingCreate - GetFileSize failed, fname = %s\n", filepath);
		CloseHandle(m_hFileDir);
		return 1;
	}

	HANDLE m_hMappingImg = CreateFileMapping(m_hFileImg, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (m_hMappingImg == nullptr) {
		printf("fileMappingCreate - CreateFileMapping failed, fname = %s\n", filepath);
		CloseHandle(m_hFileDir);
		return 1;
	}

	m_dataPtrImg = (char*)MapViewOfFile(m_hMappingImg,
		FILE_MAP_READ,
		0,
		0,
		dwFileSize);
	if (m_dataPtrImg == nullptr) {
		printf("fileMappingCreate - MapViewOfFile failed, fname = %s\n", filepath);
		CloseHandle(m_hMappingDir);
		CloseHandle(m_hFileDir);
		return 1;
	}

	return 0;
}

int IMG::Open(TCHAR* filepathImg, TCHAR *filepathDir)
{
	OpenFileDir(filepathDir);
	OpenFileImg(filepathImg);

    return 0;
}

int IMG::SaveFileById(uint32_t modelId)
{
	int err;

    err = SaveFile(m_pFilesDir[modelId].offset,
		m_pFilesDir[modelId].size,
		m_pFilesDir[modelId].name);

	if (err == 0) {
		printf("File saved with name = %s\n", m_pFilesDir[modelId].name);
	}

    return 0;
}

char* IMG::GetFilenameById(uint32_t modelId)
{
	return m_pFilesDir[modelId].name;
}

char *IMG::GetFileById(uint32_t modelId)
{
	int32_t fileSize;
	int32_t fileOffset;

	fileSize = m_pFilesDir[modelId].size * IMG_BLOCK_SIZE;
	fileOffset = m_pFilesDir[modelId].offset * IMG_BLOCK_SIZE;

	printf("[Info] ImgLoader: Loading %s file\n", m_pFilesDir[modelId].name);

	return &m_dataPtrImg[fileOffset];
}

int32_t IMG::GetFileSize(uint32_t modelId)
{
	return m_pFilesDir[modelId].size * IMG_BLOCK_SIZE;
}

int IMG::GetFileIndexByName(const char *name)
{
	int index = -1;

	for (int i = 0; i < m_countFiles; i++) {
		if (strcmp(m_pFilesDir[i].name, name) == 0) {
			index = i;
			break;
		}
	}

	if (index == -1)
		printf("[Error] File %s is not found in IMG archive\n", name);

	return index;
}

int IMG::SaveFile(int32_t offset, int32_t size, const char *name)
{
	FILE* pFile;

    int32_t fileSize;
    int32_t fileOffset;

    fileSize = size * IMG_BLOCK_SIZE;
    fileOffset = offset * IMG_BLOCK_SIZE;

    pFile = fopen(name, "wb");
    if (pFile == NULL) {
		printf("Error: cannot open file %s\n", name);
		return -1;
    }

    // write from buffer to file
    fwrite(&m_dataPtrImg[fileOffset], fileSize, 1, pFile);
    fclose(pFile);

    return 0;
}

void IMG::CleanupFileDir()
{
	UnmapViewOfFile(m_dataPtrDir);
	CloseHandle(m_hMappingDir);
	CloseHandle(m_hFileDir);
}

void IMG::CleanupFileImg()
{
	UnmapViewOfFile(m_dataPtrImg);
	CloseHandle(m_hMappingImg);
	CloseHandle(m_hFileImg);
}

void IMG::Cleanup()
{
	CleanupFileDir();
	CleanupFileImg();
}
