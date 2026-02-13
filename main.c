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
	while (keep_running)
	{
		usleep(100000);
		
	}
	
	return 0;
}
