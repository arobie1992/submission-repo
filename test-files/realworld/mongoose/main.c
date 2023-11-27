#include <stdio.h>
#include "mongoose.h"

#define UNUSED(expr) (void)expr

static int is_json_req(struct mg_http_message *hm) {
    struct mg_str *header = mg_http_get_header(hm, "Content-type");
    if(header != NULL) {
        return strcmp(header->ptr, "application/json");
    } else {
        return 0;
    }
}

static char *get_header_val(struct mg_http_message *hm, const char* header_name) {
    struct mg_str *header = mg_http_get_header(hm, header_name);
    if(header == NULL) {
        return NULL;
    }
    char *val = malloc(header->len+1);
    sprintf(val, "%.*s", (int) header->len, header->ptr);
    val[(int)(hm->method.len)] = '\0';
    return val;
}

static bool is_post(struct mg_http_message *hm) {
    char method[(int)(hm->method.len)+1];
    sprintf(method, "%.*s", (int) hm->method.len, hm->method.ptr);
    method[(int)(hm->method.len)] = '\0';
    int result = strcmp(method, "POST");
    return result == 0;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    UNUSED(fn_data);
    // POSTs with no body get sent as a chunk followed by a message
    if (ev == MG_EV_HTTP_CHUNK) {
        printf("Got chunk\n");
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        printf("Message: %s\n", hm->method);
        char *cl = get_header_val(hm, "Content-Length");
        if(cl != NULL) {
            printf("%s\n", cl);
            free(cl);
        } else {
            printf("No content-length\n");
            // mg_http_reply(c, 415, NULL, "%s\n", "No Content-Length provided");
        }
    } else if (ev == MG_EV_HTTP_MSG) {
        printf("Got message\n");
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        printf("Message: %s\n", hm->method);
        if(mg_http_match_uri(hm, "/call")) {
            if(is_post(hm)) {
                if(is_json_req(hm)) {
                    printf("Handling /call\n");
                    mg_http_reply(c, 200, NULL, "Method: %.*s\n", (int) hm->method.len, hm->method.ptr);
                } else {
                    mg_http_reply(c, 415, NULL, "%s\n", "Content-type not application/json");
                }
            } else {
                mg_http_reply(c, 405, NULL, "%s\n", "Must be POST");
            }
        } else {
            mg_http_reply(c, 404, NULL, "%s\n", "Not found");
        }
    }
}

int main() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);                                        // Init manager
    mg_http_listen(&mgr, "http://localhost:8087", fn, &mgr);  // Setup listener
    // for (;;) mg_mgr_poll(&mgr, 1000);                         // Event loop
    mg_mgr_free(&mgr);                                        // Cleanup
    printf("exiting\n");
    return 0;
}
