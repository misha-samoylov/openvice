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

enum CHUNK_TYPE {
	CHUNK_CLUMP = 0x10,
	CHUNK_STRUCT = 0x1
};

int dff_load(char *bytes);
int read_chunk_clump(char *bytes, int *offset);

#endif
