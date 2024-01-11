#include "g_local.h"

// You will need one of these for each of the requests ...
// ... if you allow concurrent requests to be sent at the same time
CURLM *stack = NULL;
size_t current_requests = 0;

void lc_discord_webhook(char* message)
{
    static request_t request;
    char json_payload[1024];

    // Don't run this if curl is disabled or the webhook URL is set to "disabled"
    if (!sv_curl_enable->value || strcmp(sv_curl_discord_chat_url->string, "disabled") == 0)
        return;

    // Use webhook.site to test curl, it's very handy!
    //char *url = "https://webhook.site/4de34388-9f3b-47fc-9074-7bdcd3cfa346";
    char* url = sv_curl_discord_chat_url->string;

    memset(&request, 0, sizeof(request_t));
    request.url = url;

    // Remove newline character from the end of message
    char* newline = strchr(message, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }

    // Format the message as a JSON payload
    snprintf(json_payload, sizeof(json_payload), "{\"content\":\"```%s```\"}", message);
    request.payload = strdup(json_payload);

    lc_start_request_function(&request);
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

    // Set the JSON payload
    curl_easy_setopt(request->handle, CURLOPT_POSTFIELDS, request->payload);

    curl_easy_setopt(request->handle, CURLOPT_WRITEFUNCTION, lc_receive_data_function);
    curl_easy_setopt(request->handle, CURLOPT_URL, request->url);
    curl_easy_setopt(request->handle, CURLOPT_WRITEDATA, request); // Passed as pvt to lc_receive_data_function
    curl_easy_setopt(request->handle, CURLOPT_PRIVATE, request); // Returned by curl_easy_getinfo with the CURLINFO_PRIVATE option
    curl_multi_add_handle(stack, request->handle);
    current_requests++;
}

void lc_parse_response(char* data)
{
	// Do something with the requested document data
}

void lc_once_per_gameframe()
{
    CURLMsg *messages;
    CURL *handle;
    request_t *request;
    int handles, remaining;

    gi.dprintf("%s: current_requests = %d\n", __func__, current_requests);
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
			curl_multi_remove_handle(stack, handle);
			curl_easy_cleanup(handle);
			current_requests--;
        }
    }
    if (handles == 0)
		current_requests = 0;
}
