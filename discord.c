#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "discord.h"
#include "config.h"
static volatile bool keep_running = true;
static pthread_t heartbeat_thread_id;
static pthread_t bot_thread_id;
static int heartbeat_interval;
static CURL *curl;
static CURL *curl_http;
static pthread_mutex_t curl_mutex;
static pthread_mutex_t http_mutex;

message_callback message_callback_fn;


void SetMessageCallback(message_callback mc)
{
	message_callback_fn = mc;
}

void parse_message_create_in(cJSON* json)
{
	if (!message_callback_fn) return;
	struct Message msg = {0};
	if (!json) return;
	cJSON* d = cJSON_GetObjectItemCaseSensitive(json, "d");
	if (!d) return;
	cJSON* Name = cJSON_GetObjectItemCaseSensitive(d, "global_name");
	cJSON* ID = cJSON_GetObjectItemCaseSensitive(d, "id");
	if ((Name && ID) && Name->valuestring && ID->valuestring)
	{
		msg.Author.Name = Name->valuestring;
		msg.Author.ID = ID->valuestring;
	}

	cJSON* content = cJSON_GetObjectItemCaseSensitive(d, "content");
	if (!content || !content->valuestring) return;
	msg.Content = content->valuestring;
	cJSON* channel = cJSON_GetObjectItemCaseSensitive(d, "channel_id");
	if (channel && channel->valuestring)
	{
		msg.ChannelID = channel->valuestring;
	}

	message_callback_fn(&msg);

}

void proccess_full_message(char* message)
{
	if (!message)
		return;

	cJSON* json = cJSON_Parse(message);
	if (!json)
		return;
	cJSON* type = cJSON_GetObjectItemCaseSensitive(json, "t");
	if (type != NULL && type->valuestring && strcmp(type->valuestring,"MESSAGE_CREATE") == 0)
		parse_message_create_in(json);

}

void discordstop()
{
	printf("\nTerminating bot...\n");
        keep_running = false;
	printf("Cleaning...\n");
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
        cJSON_AddNumberToObject(d, "intents", 33281);

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
		printf(ANSI_COLOR_BLUE "Discord:" ANSI_COLOR_RESET" %.*s\n", (int)rlen, buffer);
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

	char *full_message = NULL;
	size_t full_len = 0;

	printf("\n --- Bot is started ---\n");
        while (keep_running)
        {
		if (full_len == 0) {
                	usleep(100000);
		}

                pthread_mutex_lock(&curl_mutex);
                CURLcode res = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
                pthread_mutex_unlock(&curl_mutex);

                if (res == CURLE_OK && rlen > 0)
                {

			char *new_ptr = realloc(full_message, full_len + rlen + 1);
			if (!new_ptr) {
				fprintf(stderr, "Out of memory!\n");
				free(full_message);
				full_len = 0;
				continue;
			}
			full_message = new_ptr;

			memcpy(full_message + full_len, buffer, rlen);
			full_len += rlen;
			full_message[full_len] = '\0';
			
			if (meta->bytesleft == 0)
			{
				printf(ANSI_COLOR_BLUE "Discord:" ANSI_COLOR_RESET " %s\n", full_message);
				proccess_full_message(full_message);
				free(full_message);
				full_message = NULL;
				full_len = 0;

			}
                }
        }
	printf("\n Bot thread stopping working...");
	return NULL;
}

void send_raw_http(const char *method, const char* url, const cJSON* message)
{
	if (!curl_http || !url || !message) return;
	pthread_mutex_lock(&http_mutex);
	
	char *json_str;
	
	curl_easy_setopt(curl_http, CURLOPT_CUSTOMREQUEST, method);
	curl_easy_setopt(curl_http, CURLOPT_URL, url);
	if (message) {
		json_str = cJSON_PrintUnformatted(message);
		curl_easy_setopt(curl_http, CURLOPT_POSTFIELDS, json_str);
	} else {
		curl_easy_setopt(curl_http, CURLOPT_POSTFIELDS, NULL);
	}

	CURLcode res = curl_easy_perform(curl_http);
	if (res != CURLE_OK) {
		fprintf(stderr, ANSI_COLOR_RED "send_raw_http: Error %s" ANSI_COLOR_RESET "\n", curl_easy_strerror(res));
	}

	if (json_str) free(json_str);
	
	curl_easy_setopt(curl_http, CURLOPT_CUSTOMREQUEST, method);
	pthread_mutex_unlock(&http_mutex);
	
}

void init_http(const char* token)
{
	curl_http = curl_easy_init();
	pthread_mutex_init(&http_mutex, NULL);
	struct curl_slist *common_headers = NULL;
	char auth_header[256];
	snprintf(auth_header, sizeof(auth_header), "Authorization: Bot %s", token);
	common_headers = curl_slist_append(common_headers, auth_header);
	common_headers = curl_slist_append(common_headers, "Content-Type: application/json");

	curl_easy_setopt(curl_http, CURLOPT_HTTPHEADER, common_headers);

}

void discord_send_message(const char* channel_id, const char* message)
{	
	if (!channel_id || !message) return;

	char url[256];
	snprintf(url, sizeof(url), "https://discord.com/api/v10/channels/%s/messages", channel_id);

	cJSON* json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "content", message);

	send_raw_http("POST", url, json);
	cJSON_Delete(json);
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
	init_http(token);
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
