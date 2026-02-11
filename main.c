#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "config.h"

struct HeartbeatArgs {
	CURL *curl;
	int interval;
	pthread_mutex_t *mutex;
};

static volatile bool keep_running = true;

void handle_sigint(int sig)
{
	printf("\n%i\nTerminating bot...\n", sig);
	keep_running = false;
}

void *heartbeat_thread(void* args)
{
	struct HeartbeatArgs *hb = (struct HeartbeatArgs*)args;
	size_t sent;
	const char *ping = "{\"op\": 1, \"d\": null}";

	while(1) {
		usleep(hb->interval*1000);
		pthread_mutex_lock(hb->mutex);
		printf("[THREAD HEARTBEAT] Sending heartbeat...\n ");
		curl_ws_send(hb->curl, ping, strlen(ping), &sent, 0, CURLWS_TEXT);

		pthread_mutex_unlock(hb->mutex);
	}
	return NULL;
}

void handle_gateway(CURL* curl, int* interval_ptr)
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
						*interval_ptr = interval->valueint;
						printf("interval = %i", *interval_ptr);
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

int main ()
{
	signal(SIGINT, handle_sigint);
	config_load("config.json");
	const char *token = config_get_string("token");
	CURL* curl = curl_easy_init();
	int interval = 0;
	printf("Starting....\n");
	handle_gateway(curl, &interval);
	send_identify(curl, token);
	char buffer[4096];
	size_t rlen;
	const struct curl_ws_frame *meta;
	
	//// [THREAD HEARTBEAT]
	pthread_mutex_t curl_mutex;
	pthread_mutex_init(&curl_mutex, NULL);

	struct HeartbeatArgs hb_args = {curl, interval, &curl_mutex};
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, heartbeat_thread, &hb_args);

	////

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
	
	printf("Cleaning...\n");
	pthread_cancel(thread_id);
	config_free();
	curl_easy_cleanup(curl);
	return 0;
}
