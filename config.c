#include <stdio.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <stdbool.h>

#include "config.h"


static cJSON *root = NULL;

int config_create(const char* filename)
{
	int result = 0;
	cJSON* config = cJSON_CreateObject();
	cJSON_AddStringToObject(config, "token", "");
	cJSON_AddStringToObject(config, "GuildID", "");
	cJSON_AddBoolToObject(config, "PresenceIntent", false);
	cJSON_AddBoolToObject(config, "ServerIntent", false);
	cJSON_AddBoolToObject(config, "MessageIntent", true);

	char* string = cJSON_Print(config);

	FILE* f = fopen(filename, "w");
	if (f)
	{
		result = 0;
		fputs(string, f);
		fclose(f);
	}
	else 
	{
		result = -1;
	}

	cJSON_Delete(config);
	free(string);
	return result;

}

int config_load(const char* filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
	{
		if (config_create(filename) == 0)
		{
			f = fopen(filename, "rb");
			if (!f) return -1;
		}
	}

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

bool config_get_bool(const char* key) {
	cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
	if (item && cJSON_IsBool(item))
	{
		return cJSON_IsTrue(item);
	}
	return false;
}

void config_free() {
	if (root)
	cJSON_Delete(root);
}
