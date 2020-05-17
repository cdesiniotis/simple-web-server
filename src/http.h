#ifndef HTTP_H
#define HTTP_H

#include "str.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct{
    str message;	/* Contains full http msg (request line + headers + body). Must be freed */
    str body;		/* Points to body in message */

    str method;		/* Points to method in message */
    str uri;		/* Points to uri in message */
    str proto;		/* Points to proto in message */

    int resp_code;
    str resp_status_msg;	/* Must be freed */

    str filepath;	/* uri resource represented as file path. Must be freed */
    FILE *fp;		/* file pointer to uri resource */
    int first_resp;
} http_message_t;

// Content-types for HTTP message
extern char *content_types[];

// Index of content_types array
enum {
    TEXT = 0,
    HTML = 1,
    CSS = 2,
    JPEG = 3,		/* Used for both .jpg and .jpeg files */
    PNG = 4,
    GIF = 5
};

void free_http_message(http_message_t *hm);
void initialize_http_message(http_message_t *hm);
int get_http_request_len(unsigned char *buf, int buf_len);
void create_http_response_msg(http_message_t *hm, int resp_code, char *msg);
int parse_http_request(http_message_t *hm, char *doc_root);

#endif /* HTTP_H */
