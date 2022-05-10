#ifndef DFF_LOADER_H
#define DFF_LOADER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct header {
	uint32_t id;
	uint32_t size;
	uint32_t version;
};

enum CHUNK_TYPE {
	CHUNK_CLUMP = 0x10
};

int dff_load(char *bytes);

#endif
