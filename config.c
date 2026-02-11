#include <stdio.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include "config.h"


static cJSON *root = NULL;

int config_load(const char* filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return -1;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *data = malloc(fsize + 1);
	fread(data, 1, fsize, f);
	fclose(f);
	data[fsize] = '\0';

	root = cJSON_Parse(data);
	free(data);

	return (root != NULL) ? 0 : -1;
}

const char* config_get_string(const char* key) {

	cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
	if (cJSON_IsString(item) && (item->valuestring != NULL))
	{
		return item->valuestring;
	}
	return NULL;
}

void config_free() {
	if (root)
	cJSON_Delete(root);
}
