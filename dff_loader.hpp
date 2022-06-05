#ifndef DFF_LOADER_H
#define DFF_LOADER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct header {
	uint32_t type;
	uint32_t size;
	uint32_t version;
};

struct clump_data {
	uint32_t num_atomics;
	uint32_t num_lights;
	uint32_t num_cameras;
};

struct framelist_data {
	uint32_t num_frames;
};

struct framelist_frame {
	float rotation_matrix[3 * 3];
	float position[3];
	int32_t parent_frame_id;
	uint32_t matrix_creation_flag;
};

enum CHUNK_TYPE {
	CHUNK_CLUMP = 0x10,
	CHUNK_STRUCT = 0x1,
	CHUNK_FRAMELIST = 0xE
};

int dff_load(char *bytes);

int read_header(char *bytes, int *offset, int check_chunk_type);
int read_chunk_clump(char *bytes, int *offset);
int read_chunk_framelist(char *bytes, int *offset);

#endif
