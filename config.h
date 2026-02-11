#ifndef CONFIG_H
#define CONFIG_H

int config_load(const char* filename);

const char *config_get_string(const char* key);

int config_get_int(const char* key);

void config_free();

#endif
