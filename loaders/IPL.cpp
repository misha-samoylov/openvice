#include "IPL.h"

void IPL::Load(const char* filepath)
{
	printf("[Info] Loading: %s\n", filepath);

	FILE* fp;
	char str[512];
	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("Cannot open file %s\n", filepath);
		return;
	}

	bool isObject = false;

	while (!feof(fp)) {
		if (fgets(str, 512, fp)) {
			if (strcmp(str, "inst\n") == 0) {
				isObject = true;
			}

			if (strcmp(str, "end\n") == 0) {
				if (isObject) {
					isObject = false;
				}
			}

			int id = 0;
			char modelName[MAX_LENGTH_FILENAME]; // in file without extension (.dff)
			int interior = 0;
			float posX = 0, posY = 0, posZ = 0;
			float scale[3];
			float rot[4];

			int values = sscanf(
				str,
				"%d, %64[^,], %d, "
				"%f, %f, %f, " // pos
				"%f, %f, %f, " // scale
				"%f, %f, %f, %f", // rotation
				&id, modelName, &interior,
				&posX, &posY, &posZ,
				&scale[0], &scale[1], &scale[2],
				&rot[0], &rot[1], &rot[2], &rot[3]
			);

			if (strstr(modelName, "LOD") != NULL) {
				continue;
			}

			if (values == 13 && isObject) {

				struct IPLFile iplfile;

				iplfile.id = id;
				strcpy(iplfile.modelName, modelName);

				iplfile.interior = interior;
				/*
				 * ћен€ем положение модели в пространстве так как наша камера
				 * в Left Handed Coordinates, а движок GTA в своей координатной системе:
				 * X Ц east/west direction
				 * Y Ц north/south direction
				 * Z Ц up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				iplfile.x = posX;
				iplfile.y = posZ;
				iplfile.z = posY;

				iplfile.scale[0] = scale[0]; // y
				iplfile.scale[1] = scale[2]; // z
				iplfile.scale[2] = scale[1]; // x

				iplfile.rotation[0] = rot[0]; // y
				iplfile.rotation[1] = rot[2]; // z
				iplfile.rotation[2] = rot[1]; // x
				iplfile.rotation[3] = rot[3]; // w

				m_countObjectsInMap++;

				m_MapObjects.push_back(iplfile);
			}
		}
	}

	fclose(fp);
}