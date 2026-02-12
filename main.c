#include <signal.h>
#include <stdbool.h>

#include "config.h"
#include "discord.h"

static volatile bool keep_running = true;

void quit(int sig)
{
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
	while (keep_running)
	{
		usleep(100000);
		
	}
	
	return 0;
}
