#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <Windows.h>

#define MAX_LENGTH_FILENAME 24
#define IMG_BLOCK_SIZE 2048

struct DirEntry {
	uint32_t offset;
	uint32_t size;
	char name[MAX_LENGTH_FILENAME];
};

class IMG
{
public:
	int Open(TCHAR* filepath, TCHAR* filepathDir);
	void Cleanup();

	char* GetFilenameById(uint32_t modelId);
	char *GetFileById(uint32_t modelId);
	int GetFileIndexByName(const char *name);
	int SaveFileById(uint32_t modelId);
	int SaveFile(int32_t offset, int32_t size, const char *name);
	int32_t GetFileSize(uint32_t modelId);

private:
	int OpenFileDir(TCHAR* filepath);
	void DumpFileDir();
	int OpenFileImg(TCHAR* filepath);
	void CleanupFileDir();
	void CleanupFileImg();

	HANDLE m_hFileDir;
	HANDLE m_hMappingDir;
	unsigned char* m_dataPtrDir;
	struct DirEntry *m_pFilesDir;
	int m_countFiles;
	
	HANDLE m_hFileImg;
	HANDLE m_hMappingImg;
	char* m_dataPtrImg;
};
