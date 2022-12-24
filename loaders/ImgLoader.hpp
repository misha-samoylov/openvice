#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IMG_BLOCK_SIZE 2048

struct dirEntry {
	uint32_t offset;
	uint32_t size;
	char name[24];
};

class ImgLoader
{
public:
	int Open(const char *filepathImg, const char *filepathDir);
	void Cleanup();

	char* GetFilenameById(uint32_t id);
	char *GetFileById(uint32_t id);
	int GetFileIndexByName(const char *name);
	int SaveFileById(uint32_t id);
	int SaveFile(int32_t offset, int32_t size, const char *name);
	int32_t GetFileSize(uint32_t id);

private:
	int OpenFileDir(const char *filepathDir);
	void DumpFileDir();
	int OpenFileImg(const char *filepathImg);
	void CleanupFileDir();
	void CleanupFileImg();

	struct dirEntry *m_pFilesDir;
	FILE *m_pFileImg;
	FILE *m_pFileDir;

	int m_countFiles;
};