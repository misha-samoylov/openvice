#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#define IMG_BLOCK_SIZE 2048

struct dirEntry {
	uint32_t offset;
	uint32_t size;
	char name[24];
};

class ImgLoader
{
public:
	int Open(TCHAR* filepath, TCHAR* filepathDir);
	void Cleanup();

	char* GetFilenameById(uint32_t id);
	char *GetFileById(uint32_t id);
	int GetFileIndexByName(const char *name);
	int SaveFileById(uint32_t id);
	int SaveFile(int32_t offset, int32_t size, const char *name);
	int32_t GetFileSize(uint32_t id);

private:
	int OpenFileDir(TCHAR* filepath);
	void DumpFileDir();
	int OpenFileImg(TCHAR* filepath);
	void CleanupFileDir();
	void CleanupFileImg();

	// dir
	HANDLE m_hFileDir;
	HANDLE m_hMappingDir;
	unsigned char* m_dataPtrDir;

	struct dirEntry *m_pFilesDir;
	int m_countFiles;
	
	// img
	HANDLE m_hFileImg;
	HANDLE m_hMappingImg;
	char* m_dataPtrImg;
};