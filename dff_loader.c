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

	err = read_chunk_framelist(bytes, &offset);
	if (err) {
		printf("Error: cannot read CHUNK_FRAMELIST\n");
	}

	return err;
}

int read_header(char *bytes, int *offset, int check_chunk_type)
{
	struct header header;

	memcpy(&header, bytes + *offset, sizeof(struct header));
	*offset += sizeof(struct header);

	printf("HEADER type = %d, size = %d, version = %d\n",
		header.type, header.size, header.version);

	if (header.type != check_chunk_type) {
		printf("Error: not valid chunk. Expected: %d. Finded %d\n",
			check_chunk_type, header.type);
		return 1;
	}

	return 0;
}

int read_chunk_clump(char *bytes, int *offset)
{
	int err;

	printf("> CHUNK_CLUMP start\n");

	/* header CHUNK_CLUMP */
	err = read_header(bytes, offset, CHUNK_CLUMP);
	if (err) {
		printf("Error: cannot find CHUNK_CLUMP\n");
		return 1;
	}

	/* data CHUNK_CLUMP */
	printf("no data in CHUNK_CLUMP\n");

	/* header CHUNK_STRUCT (in CHUNK_CLUMP) */
	err = read_header(bytes, offset, CHUNK_STRUCT);
	if (err) {
		printf("Error: cannot find CHUNK_STRUCT (in CHUNK_CLUMP)\n");
		return 1;
	}

	/* data CHUNK_STRUCT (in CHUNK_CLUMP) */
	struct clump_data data;

	memcpy(&data, bytes + *offset, sizeof(struct clump_data));
	*offset += sizeof(struct clump_data);

	printf("DATA num_atomics %d, num_cameras %d, num_lights %d\n",
		data.num_atomics, data.num_cameras, data.num_lights);

	printf("> CHUNK_CLUMP end\n");

	return 0;
}

int read_chunk_framelist(char *bytes, int *offset)
{
	int err;

	printf("> CHUNK_FRAMELIST start\n");

	/* header CHUNK_FRAMELIST */
	err = read_header(bytes, offset, CHUNK_FRAMELIST);
	if (err) {
		printf("Error: cannot find CHUNK_FRAMELIST\n");
		return 1;
	}

	/* data CHUNK_FRAMELIST */
	printf("no data in CHUNK_FRAMELIST\n");

	/* header CHUNK_STRUCT (in CHUNK_FRAMELIST) */
	err = read_header(bytes, offset, CHUNK_STRUCT);
	if (err) {
		printf("Error: cannot find CHUNK_STRUCT (in CHUNK_FRAMELIST)\n");
		return 1;
	}

	/* data in CHUNK_STRUCT */
	struct framelist_data data;

	memcpy(&data, bytes + *offset, sizeof(struct framelist_data));
	*offset += sizeof(struct framelist_data);

	printf("DATA num_frames %d\n", data.num_frames);

	for (int i = 0; i < data.num_frames; i++) {
		struct framelist_frame frame;

		memcpy(&frame, bytes + *offset, sizeof(struct framelist_frame));
		*offset += sizeof(struct framelist_frame);

		printf("DATA parent_frame_id %d, matrix_creation_flag %d\n",
			frame.parent_frame_id, frame.matrix_creation_flag);
	}

	printf("> CHUNK_FRAMELIST end\n");

	return 0;
}