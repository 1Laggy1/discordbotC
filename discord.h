#ifndef DISCORD_H
#define DISCORD_H
#include <cjson/cJSON.h>

struct User {
	char* Name;
	char* ID;
};

struct Message {
	char* Content;
	char* ChannelID;
	struct User Author;
};

typedef void (*message_callback)(struct Message*);

void discord_send_message(const char* channel_id, const char* message);

void send_raw_http(const char *method, const char* url, const cJSON* message);
void SetMessageCallback(message_callback mc);
int discordstart();
void discordstop();

#endif
