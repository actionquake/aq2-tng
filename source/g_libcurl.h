#include <curl/curl.h>

#define MAX_REQUESTS         16
#define MAX_DATA_BYTES    16384

typedef struct request_s {
    char* url;
    char* payload;
    CURL* handle;
    size_t data_count;
    char data[MAX_DATA_BYTES];
} request_t;

CURLM *stack;
size_t current_requests;

