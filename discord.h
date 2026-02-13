#ifndef DISCORD_H
#define DISCORD_H


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

void SetMessageCallback(message_callback mc);
int discordstart();
void discordstop();

#endif
