#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include "config.h"
#include "discord.h"

static volatile bool keep_running = true;

void quit(int sig)
{
	printf("%i\n",sig);
	discordstop();
	keep_running = false;
}

int main ()
{
	signal(SIGINT, quit);
	if (discordstart() != 0)
	{
		quit(2);
	}
	discord_send_message("659740413476995076", "Hello!");
	discord_grant_role("401105413703073793", "1473200764460601467");
	while (keep_running)
	{
		usleep(100000);
	
	}
	
	return 0;
}
