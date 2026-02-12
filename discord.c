#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "config.h"
static volatile bool keep_running = true;
static pthread_t heartbeat_thread_id;
static pthread_t bot_thread_id;
static int heartbeat_interval;
static CURL *curl;
static pthread_mutex_t curl_mutex;

void discordstop()
{
	printf("\nTerminating bot...\n");
        keep_running = false;
	printf("Cleaning...");
	keep_running = false;
	
	if (heartbeat_thread_id) {
		pthread_join(heartbeat_thread_id, NULL);
	}

	if (bot_thread_id) {
		pthread_join(bot_thread_id, NULL);
	}
	pthread_mutex_destroy(&curl_mutex);

	config_free();
        curl_easy_cleanup(curl);
	printf("Success\n");
	
        return;
}

void send_identify(CURL* curl, const char* token)
{
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "op", 2);
        cJSON *d = cJSON_AddObjectToObject(root, "d");
        cJSON_AddStringToObject(d, "token", token);
        cJSON_AddNumberToObject(d, "intents", 513);

        cJSON *properties = cJSON_AddObjectToObject(d, "properties");
        cJSON_AddStringToObject(properties, "os", "linux");
        cJSON_AddStringToObject(properties, "browser", "Cc");
        cJSON_AddStringToObject(properties, "device", "Andryushka");

        char *json_str = cJSON_PrintUnformatted(root);
        size_t sent;
        CURLcode res = curl_ws_send(curl, json_str, strlen(json_str), &sent, 0, CURLWS_TEXT);

        if (res == CURLE_OK) {
                printf("\nIdentify sent succesfully: (%zu bytes)\n%s", sent, json_str);
        } else {
                fprintf(stderr, "\nFailed to send Identify: %s\n", curl_easy_strerror(res));
        }
                                                                                    free(json_str);
        cJSON_Delete(root);
}

void *heartbeat_thread(void* args)
{
	(void) args;
	size_t sent;
	const char *ping = "{\"op\": 1, \"d\": null}";
	int remaining = heartbeat_interval;
	while(keep_running) {
		if (remaining > 0)
		{
			usleep(100000);
			remaining -= 100;
			continue;
		}
		pthread_mutex_lock(&curl_mutex);
		printf("[THREAD HEARTBEAT] Sending heartbeat...\n ");
		curl_ws_send(curl, ping, strlen(ping), &sent, 0, CURLWS_TEXT);
		pthread_mutex_unlock(&curl_mutex);

		remaining = heartbeat_interval;
	}
	printf("Stopping heartbeat thread...\n");
	return NULL;
}

void handle_gateway(CURL* curl)
{

	const char *url = "wss://gateway.discord.gg/?v=10&encoding=json";

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		printf("Connection failed: %s\n", curl_easy_strerror(res));
		return;
	}
	printf("Connected. Waiting for Hello...\n");
	char buffer[1024];
	size_t rlen;
	const struct curl_ws_frame *meta;
	res = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
	if (res == CURLE_OK)
	{
		printf("Discord: %.*s\n", (int)rlen, buffer);
		cJSON *json = cJSON_ParseWithLength(buffer, rlen);
		if (json)
		{
			cJSON *op = cJSON_GetObjectItemCaseSensitive(json, "op");
			if (cJSON_IsNumber(op) && op->valueint == 10)
			{
				cJSON *d = cJSON_GetObjectItemCaseSensitive(json, "d");
				if (d)
				{
					cJSON *interval = cJSON_GetObjectItemCaseSensitive(d, "heartbeat_interval");
					if (cJSON_IsNumber(interval) && interval->valueint)
					{
						heartbeat_interval = interval->valueint;
						printf("interval = %i", heartbeat_interval);
						cJSON_Delete(json);
						return;
					}
				}
			}
		}
	}
	fprintf(stderr, "Error while getting answer");
	return;
}

void* bot_working(void* args)
{
	(void) args;
	char buffer[16384];
        size_t rlen;
        const struct curl_ws_frame *meta;
	printf("\n --- Bot is started ---\n");
        while (keep_running)
        {
                usleep(100000);

                pthread_mutex_lock(&curl_mutex);
                CURLcode res = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
                pthread_mutex_unlock(&curl_mutex);

                if (res == CURLE_OK)
                {
                        printf("Discord: %.*s\n", (int)rlen, buffer);

                }
        }
	printf("\n Bot thread stopping working...");
	return NULL;
}

int discordstart()
{
	config_load("config.json");
        const char *token = config_get_string("token");
        if (!token)
        {
	      printf("Failed to get token\n");
		return 1;	      
	}
		curl = curl_easy_init();
	printf("Starting....\n");
        handle_gateway(curl);
        if (heartbeat_interval < 1)
        {
                return 1;
        }
        send_identify(curl, token);

        //// [THREADS]
       	pthread_mutex_init(&curl_mutex, NULL);

        pthread_create(&heartbeat_thread_id, NULL, heartbeat_thread, NULL);
	pthread_create(&bot_thread_id, NULL, bot_working, NULL);
	////
	return 0;
}
