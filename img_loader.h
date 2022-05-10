#ifndef LOADER_IMG_H
#define LOADER_IMG_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IMG_BLOCK_SIZE 2048

struct dir_entry {
	uint32_t offset;
	uint32_t size;
	char name[24];
};

int dir_file_open(const char *filepath);
int img_file_open(const char *filepath);

int img_file_save_by_id(uint32_t id);
char *img_file_get(uint32_t id);
int img_file_save(int32_t offset, int32_t size, const char *name);

void dir_file_dump();

void dir_file_close();
void img_file_close();

#endif
