#include "dff_loader.h"

int dff_load(char *bytes)
{
	struct header clump;
	memcpy(&clump, bytes, sizeof(struct header));

	if (clump.id != CHUNK_CLUMP) {
		printf("NOT CLUMP CHUNK\n");
	} else {
		printf("CLUMP CHUNK id = %d. size = %d. version = %d\n",
			clump.id, clump.size, clump.version);
	}

	return 0;
}