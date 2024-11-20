#include "IDE.hpp"

int IDE::Load(const char* filepath)
{
	printf("[Info] Loading IDE: %s\n", filepath);

	m_countItems = 0;

	FILE* fp;
	char str[MAX_LENGTH_LINE];
	bool isObjs = false;

	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("[Error] Cannot open file: %s\n", filepath);
		return 1;
	}

	while (!feof(fp)) {
		if (fgets(str, MAX_LENGTH_LINE, fp)) {

			if (strcmp(str, "objs\n")) {
				isObjs = true;
			}

			if (isObjs && strcmp(str, "end\n") == 0) {
				isObjs = false;
			}

			int id;
			char modelName[MAX_LENGTH_FILENAME];
			char textureArchiveName[MAX_LENGTH_FILENAME];

			int values = sscanf(str, "%d, %64[^,], %64[^,]", &id, modelName, textureArchiveName);

			if (values == 3 && isObjs) {
				m_countItems++;
			}
		}
	}

	fseek(fp, 0, SEEK_SET);

	m_items = (struct itemDefinition*)malloc(sizeof(struct itemDefinition) * m_countItems);

	int i = 0;

	while (!feof(fp)) {
		if (fgets(str, MAX_LENGTH_LINE, fp)) {

			if (strcmp(str, "objs\n")) {
				isObjs = true;
			}

			if (isObjs && strcmp(str, "end\n") == 0) {
				isObjs = false;
			}

			int id;
			char modelName[MAX_LENGTH_FILENAME];
			char textureArchiveName[MAX_LENGTH_FILENAME];

			int values = sscanf(str, "%d, %64[^,], %64[^,]", &id, modelName, textureArchiveName);

			if (values == 3 && isObjs) {
				struct itemDefinition item;

				item.objectId = id;
				strcpy(item.modelName, modelName);
				strcpy(item.textureArchiveName, textureArchiveName);

				memcpy(&m_items[i], &item, sizeof(struct itemDefinition));

				i++;
			}
		}
	}

	fclose(fp);

	return 0;
}