#include "ImgLoader.hpp"

int ImgLoader::DirFileOpen(const char *filepath)
{
	int file_size;
	int count_files;

    m_pFileDir = fopen(filepath, "rb");
    if (m_pFileDir == NULL) {
		printf("Error: cannot open file %s\n", filepath);
		return -1;
    }

    fseek(m_pFileDir, 0L, SEEK_END);
    file_size = ftell(m_pFileDir);
    fseek(m_pFileDir, 0L, SEEK_SET);

    count_files = file_size / 32;

    m_pFilesDir = (struct dirEntry*)malloc(sizeof(struct dirEntry) * count_files);
    if (m_pFilesDir == NULL) {
		printf("Error: cannot allocate memory variable m_pFilesDir\n");
		return -1;
    }

    fread(m_pFilesDir, sizeof(struct dirEntry) * count_files, 1, m_pFileDir);

    return 0;
}

void ImgLoader::DirFileDump()
{
	printf("[Dump] dir file[0].name = %s\n", m_pFilesDir[0].name);
}

int ImgLoader::ImgFileOpen(const char *filepathImg)
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
	DirFileOpen(filepathDir);
	ImgFileOpen(filepathImg);

    return 0;
}

int ImgLoader::FileSaveById(uint32_t id)
{
	int err;

    err = FileSave(m_pFilesDir[id].offset,
		m_pFilesDir[id].size,
		m_pFilesDir[id].name);

	if (err == 0) {
		printf("File saved with name = %s\n", m_pFilesDir[id].name);
	}

    return 0;
}

char *ImgLoader::FileGetById(uint32_t id)
{
	char *buff;
	int32_t file_size;
	int32_t file_offset;

	file_size = m_pFilesDir[id].size * IMG_BLOCK_SIZE;
	file_offset = m_pFilesDir[id].offset * IMG_BLOCK_SIZE;

	buff = (char*)malloc(file_size);
	if (buff == NULL) {
		printf("Error: cannot allocate memory variable buff\n");
		return NULL;
	}

	/* read from img_file to buff */
	fseek(m_pFileImg, file_offset, SEEK_CUR);
	fread(buff, file_size, 1, m_pFileImg);
	fseek(m_pFileImg, 0, SEEK_CUR);

	/* do not remember to free memory */
	return buff;
}

int ImgLoader::FileSave(int32_t offset, int32_t size, const char *name)
{
    FILE *fptr;
    char *buff;
    int32_t file_size;
    int32_t file_offset;

    file_size = size * IMG_BLOCK_SIZE;
    file_offset = offset * IMG_BLOCK_SIZE;

    fptr = fopen(name, "wb");
    if (fptr == NULL) {
		printf("Error: cannot open file %s\n", name);
		return -1;
    }

    buff = (char*)malloc(file_size);
    if (buff == NULL) {
		printf("Error: cannot allocate memory variable buff\n");
		return -1;
    }
        
    /* read from img_file to buff */
    fseek(m_pFileImg, file_offset, SEEK_CUR);
    fread(buff, file_size, 1, m_pFileImg);
    fseek(m_pFileImg, 0, SEEK_CUR);

    /* write buff to file */
    fwrite(buff, file_size, 1, fptr);

    free(buff);
    fclose(fptr);

    return 0;
}

void ImgLoader::DirFileCleanup()
{
	free(m_pFilesDir);
	fclose(m_pFileDir);
}

void ImgLoader::ImgFileCleanup()
{
	fclose(m_pFileImg);
}

void ImgLoader::Cleanup()
{
	DirFileCleanup();
	ImgFileCleanup();
}