#include <jansson.h>
#include "g_local.h"

// You will need one of these for each of the requests ...
// ... if you allow concurrent requests to be sent at the same time
request_list_t *active_requests, *unused_requests;
request_list_t request_nodes[MAX_REQUESTS];
CURLM *stack = NULL;
size_t current_requests = 0;

void init_requests()
{
    size_t i;
    for (i = 0; i < MAX_REQUESTS - 1; i++)
        request_nodes[i].next = request_nodes + i + 1;
    request_nodes[MAX_REQUESTS - 1].next = NULL;
    unused_requests = request_nodes;
    active_requests = NULL;
}

request_t* new_request()
{
    request_list_t *current = unused_requests;
    if (current == NULL)
        return NULL; // Ran out of request slots
    unused_requests = unused_requests->next;
    current->next = active_requests;
    active_requests = current;
    return &current->request;
}

qboolean recycle_request(request_t* request)
{
    request_list_t *previous = NULL, *current = active_requests;
    if (current == NULL)
        return false; // No active requests exist in the list
    while (&current->request != request)
    {
        if (!current->next)
            return false; // Could not find this request in the list
        previous = current;
        current = current->next;
    }
    if (previous == NULL)
        active_requests = current->next; // Was first in the list
    else
        previous->next = current->next;
    current->next = unused_requests;
    unused_requests = current;
    memset(&current->request, 0, sizeof current->request);
    return true;
}

// This is faster than counting them and checking if it is 0
qboolean has_active_requests()
{
    return active_requests != NULL;
}

size_t count_active_requests()
{
    request_list_t *current;
    size_t count;
    for (count = 0, current = active_requests; current; current = current->next, count++)
        ; // Phatman: Don't delete this line
    return count;
}

/*
*/
void lc_get_player_stats(char* message)
{
    request_t *request;

    // Don't run this if curl is disabled or the webhook URL is set to "disabled"
    if (!sv_curl_enable->value || strcmp(sv_curl_status_api_url->string, "disabled") == 0)
        return;

    // Use webhook.site to test curl, it's very handy!
    //char *url = "https://webhook.site/4de34388-9f3b-47fc-9074-7bdcd3cfa346";
    char* url = sv_curl_status_api_url->string;

    // Get a new request object
    request = new_request();
    if (request == NULL) {
        gi.dprintf("Ran out of request slots\n");
        return;
    }

    request->url = url;
    lc_start_request_function(request);
}

/*
Call this with a string containing the message you want to send to the webhook.
Limited to 1024 chars.
*/
void lc_discord_webhook(char* message)
{
    request_t *request;
    char json_payload[1024];

    // Don't run this if curl is disabled or the webhook URL is set to "disabled"
    if (!sv_curl_enable->value || strcmp(sv_curl_discord_chat_url->string, "disabled") == 0)
        return;

    // Use webhook.site to test curl, it's very handy!
    //char *url = "https://webhook.site/4de34388-9f3b-47fc-9074-7bdcd3cfa346";
    char* url = sv_curl_discord_chat_url->string;

    // Get a new request object
    request = new_request();
    if (request == NULL) {
        gi.dprintf("Ran out of request slots\n");
        return;
    }

    // Remove newline character from the end of message
    char* newline = strchr(message, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }

    // Format the message as a JSON payload
    snprintf(json_payload, sizeof(json_payload), "{\"content\":\"```%s```\"}", message);
    request->url = url;
    request->payload = strdup(json_payload);

    lc_start_request_function(request);
}

void lc_shutdown_function()
{
    if (stack)
    {
        curl_multi_cleanup(stack);
        stack = NULL;
    }
	current_requests = 0;
    curl_global_cleanup();
}

qboolean lc_init_function()
{
    lc_shutdown_function();
    init_requests();
    if (curl_global_init(CURL_GLOBAL_ALL))
        return false;
    stack = curl_multi_init();
    if (!stack)
        return false;
    if (curl_multi_setopt(stack, CURLMOPT_MAXCONNECTS, MAX_REQUESTS) != CURLM_OK)
    {
        curl_multi_cleanup(stack);
        curl_global_cleanup();
        return false;
    }
	// ...
	return true;
}

// Your callback function's return type and parameter types _must_ match this:
size_t lc_receive_data_function(char *data, size_t blocks, size_t bytes, void *pvt)
{
	request_t *request;
    if (bytes <= 0)
        return 0;
	request = (request_t*) pvt;
	if (!request)
	{
        gi.dprintf("%s: ERROR! pvt argument was NULL.\n", __func__);
		return 0;
	}
    if (bytes > MAX_DATA_BYTES - request->data_count)
	{
        gi.dprintf("%s: ERROR! Too much data.\n", __func__);
        return 0; // TODO: Ensure this cancels the request
	}
    memcpy(request->data + request->data_count, request->data, bytes);
    request->data_count += bytes;
    return bytes;
}

// Requires that request->url already be set to the target URL
void lc_start_request_function(request_t* request)
{
    struct curl_slist *headers = NULL;

    request->data_count = 0;
    request->handle = curl_easy_init();

    // Set the headers to indicate we're sending JSON data
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(request->handle, CURLOPT_HTTPHEADER, headers);

    // Set the JSON payload if it exists
    if (request->payload)
        curl_easy_setopt(request->handle, CURLOPT_POSTFIELDS, request->payload);

    curl_easy_setopt(request->handle, CURLOPT_WRITEFUNCTION, lc_receive_data_function);
    curl_easy_setopt(request->handle, CURLOPT_URL, request->url);
    curl_easy_setopt(request->handle, CURLOPT_WRITEDATA, request); // Passed as pvt to lc_receive_data_function
    curl_easy_setopt(request->handle, CURLOPT_PRIVATE, request); // Returned by curl_easy_getinfo with the CURLINFO_PRIVATE option
    curl_multi_add_handle(stack, request->handle);
    current_requests++;
    free(request->payload); // Frees up the memory allocated by strdup
}

void process_stats(json_t *stats_json)
{
    if (!json_is_object(stats_json)) {
        gi.dprintf(stderr, "error: stats is not a JSON object\n");
        return;
    }

    json_t *frags_json = json_object_get(stats_json, "frags");
    json_t *deaths_json = json_object_get(stats_json, "deaths");

    if (!frags_json || !deaths_json) {
        gi.dprintf(stderr, "error: frags or deaths is missing\n");
        return;
    }

    if (!json_is_integer(frags_json) || !json_is_integer(deaths_json)) {
        gi.dprintf(stderr, "error: frags or deaths is not an integer\n");
        return;
    }

    int frags = json_integer_value(frags_json);
    int deaths = json_integer_value(deaths_json);

    lt_stats_t stats;
    stats.total_kills = frags;
    stats.total_deaths = deaths;

    // Do something with stats
}

void lc_parse_response(char* data)
{
	json_error_t error;
    json_t *root = json_loads(data, 0, &error);

    if(!root)
    {
        gi.dprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return;
    }

    json_t *stats_json = json_object_get(root, "stats");
    if(!json_is_object(stats_json))
    {
        // If the root is not "stats", do nothing
        json_decref(root);
        return;
    } else {
        //process_stats(stats_json);
        json_decref(root);
    }
}

void lc_once_per_gameframe()
{
    CURLMsg *messages;
    CURL *handle;
    request_t *request;
    int handles, remaining;

    // Debug request counts
    //gi.dprintf("%s: current_requests = %d\n", __func__, current_requests);
    if (current_requests <= 0)
        return;
    if (curl_multi_perform(stack, &handles) != CURLM_OK)
    {
        gi.dprintf("%s: curl_multi_perform error -- resetting libcurl session\n", __func__);
        lc_init_function();
        return;
    }
    while ((messages = curl_multi_info_read(stack, &remaining)))
    {
        handle = messages->easy_handle;
        if (messages->msg == CURLMSG_DONE)
        {
            curl_easy_getinfo(handle, CURLINFO_PRIVATE, &request);
            if (request->data_count < MAX_DATA_BYTES)
                request->data[request->data_count] = '\0';
            else
                request->data[MAX_DATA_BYTES - 1] = '\0';
			lc_parse_response(request->data);
            recycle_request(request);
            curl_multi_remove_handle(stack, handle);
            curl_easy_cleanup(handle);
            current_requests--;
        }
    }
    free(request->payload); // Frees up the memory allocated by strdup
    if (handles == 0)
		current_requests = 0;
}
