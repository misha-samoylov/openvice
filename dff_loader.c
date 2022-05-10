#include "dff_loader.h"

int dff_load(char *bytes)
{
	int offset;
	int err;

	offset = 0;
	err = 0;

	err = read_chunk_clump(bytes, &offset);
	if (err) {
		printf("Error: cannot read CHUNK_CLUMP\n");
	}

	return err;
}

int read_chunk_clump(char *bytes, int *offset)
{
	/* read header CHUNK_CLUMP */
	struct header chunk_clump;
	memcpy(&chunk_clump, bytes, sizeof(struct header));
	*offset += sizeof(struct header);

	printf("HEADER type = %d. size = %d. version = %d\n",
		chunk_clump.type, chunk_clump.size, chunk_clump.version);

	if (chunk_clump.type != CHUNK_CLUMP) {
		printf("Error: cannot find CHUNK_CLUMP\n");
		return 1;
	}

	/* no data in CHUNK_CLUMP */
	printf("no data in CHUNK_CLUMP\n");

	/* read header CHUNK_CLUMP -> CHUNK_STRUCT */
	struct header chunk_struct;

	memcpy(&chunk_struct, bytes + *offset, sizeof(struct header));
	*offset += sizeof(struct header);

	printf("HEADER type = %d. size = %d. version = %d\n",
		chunk_struct.type, chunk_struct.size, chunk_struct.version);

	if (chunk_struct.type != CHUNK_STRUCT) {
		printf("Error: cannot find CHUNK_STRUCT in CHUNK_CLUMP\n");
		return 1;
	}

	/* read data CHUNK_CLUMP -> CHUNK_STRUCT */
	struct clump_data data;
	memcpy(&data, bytes + *offset, sizeof(struct clump_data));
	*offset += sizeof(struct header);

	printf("DATA num_atomics %d. num_cameras %d. num_lights %d\n",
		data.num_atomics, data.num_cameras, data.num_lights);

	return 0;
}