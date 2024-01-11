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

extern CURLM *stack;
extern size_t current_requests;

