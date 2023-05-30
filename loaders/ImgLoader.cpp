#include "ImgLoader.hpp"

#include <Windows.h>

int ImgLoader::OpenFileDir(TCHAR *filepath)
{
	int fileSize;

	HANDLE m_hFileDir = CreateFile(filepath, GENERIC_READ, 0, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	DWORD dwFileSize = ::GetFileSize(m_hFileDir, nullptr);

	HANDLE m_hMappingDir = CreateFileMapping(m_hFileDir, nullptr, PAGE_READONLY, 0, 0, nullptr);

	// TODO: Check on errors

	m_dataPtrDir = (unsigned char*)MapViewOfFile(m_hMappingDir,
		FILE_MAP_READ,
		0,
		0,
		dwFileSize
	);

    fileSize = dwFileSize;
	m_countFiles = fileSize / 32;

	m_pFilesDir = (struct dirEntry*)m_dataPtrDir;

    return 0;
}

void ImgLoader::DumpFileDir()
{
	printf("[Dump] dir file[0].name = %s\n", m_pFilesDir[0].name);
}

int ImgLoader::OpenFileImg(TCHAR* filepath)
{
	HANDLE m_hFileImg = CreateFile(filepath, GENERIC_READ, 0, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	DWORD dwFileSize = ::GetFileSize(m_hFileImg, nullptr);

	HANDLE m_hMappingImg = CreateFileMapping(m_hFileImg, nullptr, PAGE_READONLY, 0, 0, nullptr);

	// TODO: Check on errors

	m_dataPtrImg = (char*)MapViewOfFile(m_hMappingImg,
		FILE_MAP_READ,
		0,
		0,
		dwFileSize);

	return 0;
}

int ImgLoader::Open(TCHAR* filepathImg, TCHAR *filepathDir)
{
	OpenFileDir(filepathDir);
	OpenFileImg(filepathImg);

    return 0;
}

int ImgLoader::SaveFileById(uint32_t id)
{
	int err;

    err = SaveFile(m_pFilesDir[id].offset,
		m_pFilesDir[id].size,
		m_pFilesDir[id].name);

	if (err == 0) {
		printf("File saved with name = %s\n", m_pFilesDir[id].name);
	}

    return 0;
}

char* ImgLoader::GetFilenameById(uint32_t id)
{
	return m_pFilesDir[id].name;
}

char *ImgLoader::GetFileById(uint32_t id)
{
	int32_t fileSize;
	int32_t fileOffset;

	fileSize = m_pFilesDir[id].size * IMG_BLOCK_SIZE;
	fileOffset = m_pFilesDir[id].offset * IMG_BLOCK_SIZE;

	printf("[NOTICE] ImgLoader: Loading %s file\n", m_pFilesDir[id].name);

	return &m_dataPtrImg[fileOffset];
}

int32_t ImgLoader::GetFileSize(uint32_t id)
{
	return m_pFilesDir[id].size * IMG_BLOCK_SIZE;
}

int ImgLoader::GetFileIndexByName(const char *name)
{
	int index = -1;

	for (int i = 0; i < m_countFiles; i++) {
		if (strcmp(m_pFilesDir[i].name, name) == 0) {
			index = i;
			break;
		}
	}

	if (index == -1)
		printf("File %s is not found in IMG archive\n", name);

	return index;
}

int ImgLoader::SaveFile(int32_t offset, int32_t size, const char *name)
{
    /*
	FILE* pFile;
    char *buff;
    int32_t fileSize;
    int32_t fileOffset;

    fileSize = size * IMG_BLOCK_SIZE;
    fileOffset = offset * IMG_BLOCK_SIZE;

    pFile = fopen(name, "wb");
    if (pFile == NULL) {
		printf("Error: cannot open file %s\n", name);
		return -1;
    }

    buff = (char*)malloc(fileSize);
    if (buff == NULL) {
		printf("Error: cannot allocate memory variable buff\n");
		return -1;
    }

    // read from img_file to buff
    fseek(m_pFileImg, fileOffset, SEEK_SET);
    fread(buff, fileSize, 1, m_pFileImg);
    fseek(m_pFileImg, 0, SEEK_CUR);

    // write buff to file
    fwrite(buff, fileSize, 1, pFile);

    free(buff);
    fclose(pFile);
	*/

    return 0;
}

void ImgLoader::CleanupFileDir()
{
	UnmapViewOfFile(m_dataPtrDir);
	CloseHandle(m_hMappingDir);
	CloseHandle(m_hFileDir);
}

void ImgLoader::CleanupFileImg()
{
	UnmapViewOfFile(m_dataPtrImg);
	CloseHandle(m_hMappingImg);
	CloseHandle(m_hFileImg);
}

void ImgLoader::Cleanup()
{
	CleanupFileDir();
	CleanupFileImg();
}
