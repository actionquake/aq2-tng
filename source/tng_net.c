/*

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// tng_net.c - CURL integration for various uses
//

#include "g_local.h"
#include <curl/curl.h>

#ifndef WIN32
#include <pthread.h>
#endif

static qboolean     curl_initialized;
int handle_count = 1;

struct thread_data_t {
    int payloadType;
    char *payload;
};
struct thread_data_t thread_data_array[1];

/*
===============
HTTP_Init

Init libcurl.
===============
*/
void cURL_Init(void)
{
    if (curl_global_init(CURL_GLOBAL_NOTHING)) {
        Com_Printf("curl_global_init failed\n");
        return;
    }

    curl_initialized = true;
    Com_Printf("%s initialized.\n", curl_version());
}

void cURL_Shutdown(void)
{
    if (!curl_initialized)
        return;

    curl_global_cleanup();
    curl_initialized = false;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}

void cURL_CallSendMsgThread(int payloadType, const char *payload, ...)
{
    #ifndef WIN32
    pthread_t thread;
    char text[512];
    va_list argptr;

    va_start (argptr, payload);
    vsnprintf (text, sizeof(text), payload, argptr);
    va_end (argptr);

    thread_data_array->payloadType = payloadType;
    thread_data_array->payload = text;

    
	pthread_create(&thread, NULL, cURL_SendMsg, (void *) &thread_data_array);
	pthread_exit(NULL);
    #else
    // Not supporting Win32 at this time for threading
    return;
	#endif
}

void cURL_SendMsg(void *threadargs)
{
    int payloadType;
	char text[1024];
    char jsonmsg[1024];
    char url[512];

    struct thread_data_t *data;
    data = (struct thread_data_t *) threadargs;
    data->payload = text;
    data->payloadType = payloadType;

    /// Sanity checks before we do anything
    // If sv_curl_enable is disabled, take a hike
	if (!sv_curl_enable->value){
		return 0;
	}
    
    /// So far so good, time to go to work
    CURL *curl = curl_easy_init();

    if (payloadType == CURL_STATUS_API) {
        Q_strncpyz(url, sv_curl_status_api_url->string, sizeof(url));
        if (strcmp(url,"disabled") == 0) {
            return 0;
        }
        // We ignore the payload here, it is not used, because we are populating the data derived from cvars
        Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"status\": {\"hostname\": \"%s\", \"port\": \"%s\", \"dmflags\": \"%s\", \"gamemode\": \"%i\", \"gamemodeflags\": \"%i\", \"actionversion\": \"%s\", \"maxclients\": \"%s\", \"fraglimit\": \"%s\", \"timelimit\": \"%s\", \"roundlimit\": \"%s\", \"roundtimelimit\": \"%s\", \"dm_choose\": \"%s\", \"tgren\": \"%s\", \"e_enhancedSlippers\": \"%s\", \"server_id\": \"%s\", \"stat_logs\": \"%s\", \"sv_antilag\": \"%s\", \"sv_antilag_interp\": \"%s\", \"g_spawn_items\": \"%s\", \"ltk_loadbots\": \"%s\"}}", 
        hostname->string,
        net_port->string,
        dmflags->string,
        Gamemode(),
        Gamemodeflag(),
        actionversion->string,
        maxclients->string,
        fraglimit->string,
        timelimit->string,
        roundlimit->string,
        roundtimelimit->string,
        dm_choose->string,
        tgren->string,
        e_enhancedSlippers->string,
        server_id->string,
        stat_logs->string,
        sv_antilag->string,
        sv_antilag_interp->string,
        g_spawn_items->string,
        ltk_loadbots->string
        );
    } 
    
    if (payloadType == CURL_DISCORD_CHAT) // Send message to Discord Webhook URL
    {
        Q_strncpyz(url, sv_curl_discord_chat_url->string, sizeof(url));
        if (strcmp(url,"disabled") == 0) {
            return 0;
        }
        text[strcspn(text, "\n")] = 0;
        Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"content\": \"```(%s) - %s```\"}", hostname->string, text);
    }
    
    if (payloadType == CURL_AWS_API)
    {
        text[strcspn(text, "\n")] = 0;
        Com_sprintf(jsonmsg, sizeof(jsonmsg), "{\"aqtionapi\": \"```(%s) - %s```\"}", hostname->string, text);
    }
    
    if (payloadType > CURL_ENDPOINTS_MAX)
    {
        // Invalid payloadType supplied, do nothing
        gi.dprintf("cURL_SendMsg error: invalid payloadType %i", payloadType);
        return 0;
    }
    
    // Send this off to be consumed by curl
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonmsg);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Do nothing if we don't have a handle to process
    // if(handle_count < 1){
    //     return;
    // }

    // !!!
    // Debug area //
    // Do not print responses from curl request
    // Comment this line...
    //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    // ...and uncomment this line for full debug mode
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    // End Debug //

    	// cURL poll

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}