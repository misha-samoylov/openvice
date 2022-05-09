#include "img_loader.h"

struct dir_entry* files_dir;

FILE* fptr_img;
FILE* fptr_dir;

int dir_file_open(const char *filepath)
{
	int file_size;
	int count_files;

    fptr_dir = fopen(filepath, "rb");
    if (fptr_dir == NULL) {
		printf("Cannot open file %s\n", filepath);
		return -1;
    }

    fseek(fptr_dir, 0L, SEEK_END);
    file_size = ftell(fptr_dir);
    fseek(fptr_dir, 0L, SEEK_SET);

    count_files = file_size / 32;

    files_dir = (struct dir_entry*)malloc(sizeof(struct dir_entry) * count_files);
    if (files_dir == NULL) {
		printf("Cannot allocate memory var files_dir\n");
		return -1;
    }

    fread(files_dir, sizeof(struct dir_entry) * count_files, 1, fptr_dir);

    printf("file name = %s\n", files_dir[0].name);

    return 0;
}

void dir_file_close()
{
    free(files_dir);
    fclose(fptr_dir);
}

int img_file_open(const char *filepath)
{
    fptr_img = fopen(filepath, "rb");
    if (fptr_img == NULL) {
		printf("Cannot open file %s\n", filepath);
		return -1;
    }

    return 0;
}

int img_file_save_by_id(uint32_t id)
{
    img_file_save(files_dir[id].offset,
		files_dir[id].size,
		files_dir[id].name);

    return 0;
}

int img_file_save(int32_t offset, int32_t size, const char *name)
{
    FILE *fptr;
    char *buff;
    int32_t file_size;
    int32_t file_offset;

    file_size = size * IMG_BLOCK_SIZE;
    file_offset = offset * IMG_BLOCK_SIZE;

    fptr = fopen(name, "wb");
    if (fptr == NULL) {
		printf("Cannot open file %s\n", name);
		return -1;
    }

    buff = (char*)malloc(file_size);
    if (buff == NULL) {
		printf("Cannot allocate memory var buff\n");
		return -1;
    }
        
    /* read from img_file to buff */
    fseek(fptr_img, file_offset, SEEK_CUR);
    fread(buff, file_size, 1, fptr_img);
    fseek(fptr_img, 0, SEEK_CUR);

    /* write buff to file */
    fwrite(buff, file_size, 1, fptr);

    free(buff);
    fclose(fptr);

    return 0;
}

void img_file_close()
{
    fclose(fptr_img);
}
