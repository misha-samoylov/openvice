#include "IDE.hpp"

enum IdeSection {
	IDE_SECTION_NONE = 0,
	IDE_SECTION_OBJS,
	IDE_SECTION_TOBJ
};

static void TrimInPlace(char* s)
{
	if (!s)
		return;
	char* start = s;
	while (*start == ' ' || *start == '\t')
		start++;
	if (start != s)
		memmove(s, start, strlen(start) + 1);
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
		s[n - 1] = '\0';
		n--;
	}
}

static bool ParseObjsOrTobjLine(const char* str, bool timed, itemDefinition* out)
{
	int id = 0;
	char modelName[64];
	char textureArchiveName[64];
	int numObjs = 1;
	float dist0 = 0.0f, dist1 = 0.0f, dist2 = 0.0f;
	int flags = 0;
	int timeOn = 0, timeOff = 24;

	modelName[0] = '\0';
	textureArchiveName[0] = '\0';

	/*
	 * re3 CFileLoader::LoadObject / LoadTimeObject formats (comma-separated in VC):
	 *   objs 1: id, model, txd, numObjs, dist, flags
	 *   objs 2: id, model, txd, numObjs, dist0, dist1, flags
	 *   objs 3: id, model, txd, numObjs, dist0, dist1, dist2, flags
	 *   tobj *: same, then timeOn, timeOff after flags
	 */
	int values = 0;
	if (timed) {
		values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %f, %f, %d, %d, %d",
			&id, modelName, textureArchiveName, &numObjs,
			&dist0, &dist1, &dist2, &flags, &timeOn, &timeOff);
		if (values == 10) {
			/* 3 draw distances */
		} else {
			values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %f, %d, %d, %d",
				&id, modelName, textureArchiveName, &numObjs,
				&dist0, &dist1, &flags, &timeOn, &timeOff);
			if (values == 9) {
				/* 2 draw distances */
			} else {
				values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %d, %d, %d",
					&id, modelName, textureArchiveName, &numObjs,
					&dist0, &flags, &timeOn, &timeOff);
				if (values != 8)
					return false;
			}
		}
	} else {
		values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %f, %f, %d",
			&id, modelName, textureArchiveName, &numObjs,
			&dist0, &dist1, &dist2, &flags);
		if (values == 8) {
			/* 3 draw distances */
		} else {
			values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %f, %d",
				&id, modelName, textureArchiveName, &numObjs,
				&dist0, &dist1, &flags);
			if (values == 7) {
				/* 2 draw distances */
			} else {
				values = sscanf(str, "%d, %63[^,], %63[^,], %d, %f, %d",
					&id, modelName, textureArchiveName, &numObjs,
					&dist0, &flags);
				if (values != 6)
					return false;
			}
		}
	}

	TrimInPlace(modelName);
	TrimInPlace(textureArchiveName);
	if (modelName[0] == '\0' || textureArchiveName[0] == '\0')
		return false;

	memset(out, 0, sizeof(*out));
	out->objectId = id;
	strncpy(out->modelName, modelName, MAX_LENGTH_FILENAME - 1);
	strncpy(out->textureArchiveName, textureArchiveName, MAX_LENGTH_FILENAME - 1);
	out->flags = (uint32_t)flags;
	out->isTimed = timed;
	out->timeOn = timed ? timeOn : 0;
	out->timeOff = timed ? timeOff : 24;
	return true;
}

int IDE::Load(const char* filepath)
{
	printf("[Info] Loading IDE: %s\n", filepath);

	m_countItems = 0;
	m_items = nullptr;

	FILE* fp;
	char str[MAX_LENGTH_LINE];

	if ((fp = fopen(filepath, "r")) == NULL) {
		printf("[Error] Cannot open file: %s\n", filepath);
		return 1;
	}

	/* Count objs + tobj entries. */
	IdeSection section = IDE_SECTION_NONE;
	while (fgets(str, MAX_LENGTH_LINE, fp)) {
		TrimInPlace(str);
		if (str[0] == '\0' || str[0] == '#')
			continue;
		if (_stricmp(str, "objs") == 0) {
			section = IDE_SECTION_OBJS;
			continue;
		}
		if (_stricmp(str, "tobj") == 0) {
			section = IDE_SECTION_TOBJ;
			continue;
		}
		if (_stricmp(str, "end") == 0) {
			section = IDE_SECTION_NONE;
			continue;
		}
		if (section != IDE_SECTION_OBJS && section != IDE_SECTION_TOBJ)
			continue;

		itemDefinition tmp;
		if (ParseObjsOrTobjLine(str, section == IDE_SECTION_TOBJ, &tmp))
			m_countItems++;
	}

	if (m_countItems <= 0) {
		fclose(fp);
		return 0;
	}

	fseek(fp, 0, SEEK_SET);
	m_items = (struct itemDefinition*)malloc(sizeof(struct itemDefinition) * m_countItems);
	int i = 0;
	section = IDE_SECTION_NONE;

	while (fgets(str, MAX_LENGTH_LINE, fp)) {
		TrimInPlace(str);
		if (str[0] == '\0' || str[0] == '#')
			continue;
		if (_stricmp(str, "objs") == 0) {
			section = IDE_SECTION_OBJS;
			continue;
		}
		if (_stricmp(str, "tobj") == 0) {
			section = IDE_SECTION_TOBJ;
			continue;
		}
		if (_stricmp(str, "end") == 0) {
			section = IDE_SECTION_NONE;
			continue;
		}
		if (section != IDE_SECTION_OBJS && section != IDE_SECTION_TOBJ)
			continue;
		if (i >= m_countItems)
			break;

		if (ParseObjsOrTobjLine(str, section == IDE_SECTION_TOBJ, &m_items[i]))
			i++;
	}

	m_countItems = i;
	fclose(fp);

	printf("[Info] IDE %s: %d objects\n", filepath, m_countItems);
	return 0;
}
