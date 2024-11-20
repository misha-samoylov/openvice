#include "IPL.hpp"

int IPL::Load(const char* filepath)
{
	FILE* fp;
	char str[MAX_LENGTH_LINE];
	bool isObject = false;

	printf("[Info] Loading IPL: %s\n", filepath);

	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("[Error] Cannot open file: %s\n", filepath);
		return 1;
	}

	while (!feof(fp)) {
		if (fgets(str, MAX_LENGTH_LINE, fp)) {
			if (strcmp(str, "inst\n") == 0) {
				isObject = true;
			}

			if (strcmp(str, "end\n") == 0) {
				if (isObject) {
					isObject = false;
				}
			}

			int id = 0;
			char modelName[MAX_LENGTH_FILENAME];
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

			if (values == 13 && isObject) {
				m_countItems++;
			}
		}
	}

	fseek(fp, 0, SEEK_SET);

	m_mapItems = (struct mapItem*)malloc(sizeof(struct mapItem) * m_countItems);

	int i = 0;

	while (!feof(fp)) {
		if (fgets(str, MAX_LENGTH_LINE, fp)) {
			if (strcmp(str, "inst\n") == 0) {
				isObject = true;
			}

			if (strcmp(str, "end\n") == 0) {
				if (isObject) {
					isObject = false;
				}
			}

			int id = 0;
			char modelName[MAX_LENGTH_FILENAME];
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

			if (values == 13 && isObject) {

				struct mapItem item;

				item.id = id;
				strcpy(item.modelName, modelName);

				item.interior = interior;
				/*
				 * ћен€ем положение модели в пространстве так как наша камера
				 * в Left Handed Coordinates, а движок GTA в своей координатной системе:
				 * X Ц east/west direction
				 * Y Ц north/south direction
				 * Z Ц up/down direction
				 * @see https://gtamods.com/wiki/Map_system
				*/
				item.x = posX;
				item.y = posZ;
				item.z = posY;

				item.scale[0] = scale[0]; // y
				item.scale[1] = scale[2]; // z
				item.scale[2] = scale[1]; // x

				item.rotation[0] = rot[0]; // y
				item.rotation[1] = rot[2]; // z
				item.rotation[2] = rot[1]; // x
				item.rotation[3] = rot[3]; // w

				memcpy(&m_mapItems[i], &item, sizeof(struct mapItem));

				i++;
			}
		}
	}

	fclose(fp);

	return 0;
}