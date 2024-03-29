#include "ImgLoader.hpp"

int ImgLoader::OpenFileDir(const char *filepath)
{
	int fileSize;

    m_pFileDir = fopen(filepath, "rb");
    if (m_pFileDir == NULL) {
		printf("Error: cannot open file %s\n", filepath);
		return -1;
    }

    fseek(m_pFileDir, 0L, SEEK_END);
    fileSize = ftell(m_pFileDir);
    fseek(m_pFileDir, 0L, SEEK_SET);

	m_countFiles = fileSize / 32;

    m_pFilesDir = (struct dirEntry*)malloc(sizeof(struct dirEntry) * m_countFiles);
    if (m_pFilesDir == NULL) {
		printf("Error: cannot allocate memory variable m_pFilesDir\n");
		return -1;
    }

    fread(m_pFilesDir, sizeof(struct dirEntry) * m_countFiles, 1, m_pFileDir);

    return 0;
}

void ImgLoader::DumpFileDir()
{
	printf("[Dump] dir file[0].name = %s\n", m_pFilesDir[0].name);
}

int ImgLoader::OpenFileImg(const char *filepathImg)
{
	m_pFileImg = fopen(filepathImg, "rb");

	if (m_pFileImg == NULL) {
		printf("Error: cannot open file %s\n", filepathImg);
		return -1;
	}

	return 0;
}

int ImgLoader::Open(const char *filepathImg, const char *filepathDir)
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
	char *buff;
	int32_t fileSize;
	int32_t fileOffset;

	fileSize = m_pFilesDir[id].size * IMG_BLOCK_SIZE;
	fileOffset = m_pFilesDir[id].offset * IMG_BLOCK_SIZE;

	printf("[NOTICE] ImgLoader: Loading %s file\n", m_pFilesDir[id].name);

	buff = (char*)malloc(fileSize);
	if (buff == NULL) {
		printf("Error: cannot allocate memory variable buff\n");
		return NULL;
	}

	/* read from img_file to buff */
	fseek(m_pFileImg, fileOffset, SEEK_SET);
	fread(buff, fileSize, 1, m_pFileImg);

	/* do not remember to free memory */
	return buff;
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
    FILE *pFile;
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

    /* read from img_file to buff */
    fseek(m_pFileImg, fileOffset, SEEK_SET);
    fread(buff, fileSize, 1, m_pFileImg);
    fseek(m_pFileImg, 0, SEEK_CUR);

    /* write buff to file */
    fwrite(buff, fileSize, 1, pFile);

    free(buff);
    fclose(pFile);

    return 0;
}

void ImgLoader::CleanupFileDir()
{
	free(m_pFilesDir);
	fclose(m_pFileDir);
}

void ImgLoader::CleanupFileImg()
{
	fclose(m_pFileImg);
}

void ImgLoader::Cleanup()
{
	CleanupFileDir();
	CleanupFileImg();
}