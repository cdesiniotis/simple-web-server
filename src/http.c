#include "http.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

char *content_types[] = {"text/plain", "text/html", "text/css",
			 "image/jpeg", "image/png", "image/gif"};

void free_http_message(http_message_t *hm)
{
    DBG(fprintf(stderr, "[INFO] freeing contents for http msg\n"));
    // NOTE: only message and resp_status_msg are allocated on the heap.
    // The rest are pointers to the message memory block
    DBG(fprintf(stderr, "[INFO] freeing hm->message\n"));
    free_str(&hm->message);
    hm->body.p = hm->method.p = hm->uri.p = hm->proto.p = NULL;

    DBG(fprintf(stderr, "[INFO] freeing hm->resp_status_msg\n"));
    free_str(&hm->resp_status_msg);

    DBG(fprintf(stderr, "[INFO] freeing hm->filepath\n"));
    free_str(&hm->filepath);

    DBG(fprintf(stderr, "[INFO] closing hm->fp\n"));
    if (hm->fp != NULL) {
	DBG(fprintf(stderr, "[INFO] closing file"));
	fclose(hm->fp);
	hm->fp = NULL;
    }

    DBG(fprintf(stderr, "[INFO] freeing http msg struct\n"));
    if (hm != NULL) {
	free(hm);
	hm = NULL;
    }
}

void initialize_http_message(http_message_t *hm)
{
    hm->first_resp = 1;
    hm->resp_status_msg.p = NULL;
    hm->filepath.p = NULL;
    hm->fp = NULL;
}

// -1 if request is malformed
// 0 if request is not complete yet
// >0 request length, including the last \r\n\r\n
int get_http_request_len(unsigned char *buf, int buf_len)
{
    int i;
    for (i = 0; i < buf_len; ++i) {
	if (!isprint(buf[i]) && buf[i] != '\r' && buf[i] != '\n')
	    return -1;
	else if (buf[i] == '\n' && i+2 < buf_len && buf[i+1] == '\r' && buf[i+2] == '\n')
	    return i+3;
    }
    return 0;
}


void create_http_response_msg(http_message_t *hm, int resp_code, char *msg)
{
    hm->resp_code = resp_code;
    int len = strlen(msg);
    hm->resp_status_msg.p = (char*) malloc(len * sizeof(char));
    hm->resp_status_msg.len = len;
    memset(hm->resp_status_msg.p, 0, len);
    strncpy(hm->resp_status_msg.p, msg, len);
}

// 1 if resp_code 200
// 0 otherwise
// This allows the caller to know whether to send a payload or not
int parse_http_request(http_message_t *hm, char *doc_root)
{
    int req_len = get_http_request_len(hm->message.p, hm->message.len);
    DBG(fprintf(stderr, "[INFO] http request len: %d\n", req_len));
    if (req_len == -1) {
	// malinformed request
	create_http_response_msg(hm, 400, "Bad Request");
	return 0;
    } else if (req_len < hm->message.len){
	hm->message.len = req_len;
    }

    // Parse the request line
    char *end_req_line = strpbrk(hm->message.p, "\r\n");	/* point to end of request line */
    char *cur = hm->message.p;
    char *end = hm->message.p + hm->message.len;
    cur = skip(cur, end, " ", &hm->method);
    cur = skip(cur, end, " ", &hm->uri);
    cur = skip(cur, end, "\r\n", &hm->proto);

    if (hm->uri.p <= hm->method.p || hm->proto.p <= hm->uri.p || hm->proto.p > end_req_line) {
	// Malinformed request line
	create_http_response_msg(hm, 400, "Bad Request");
	return 0;
    }

    // DEBUG
    DBG(fprintf(stderr, "[INFO] http req method: %.*s\n", (int) hm->method.len, hm->method.p));
    DBG(fprintf(stderr, "[INFO] http req uri: %.*s\n", (int) hm->uri.len, hm->uri.p));
    DBG(fprintf(stderr, "[INFO] http req proto: %.*s\n", (int) hm->proto.len, hm->proto.p));

    // Check method
    // NOTE: Only support the GET method
    if (hm->method.len != 3 || strncmp("GET", hm->method.p, 3) != 0) {
	// invalid method
	create_http_response_msg(hm, 501, "Not Implemented");
	return 0;
    }

    // Check uri
    // Allocate memory block for filepath string
    // filepath is '.' + uri
    int doc_root_len = strlen(doc_root);
    if (hm->uri.p[hm->uri.len - 1] == '/') {
	// uri is '/'
	// set <document_root>/index.html as the filepath
	DBG(fprintf(stderr, "[INFO] PREVIOUS CONTENTS OF FILEPATH: %s %d\n", hm->filepath.p, (int) hm->filepath.len));
	DBG(fprintf(stderr, "[INFO] DOC ROOT: %s\n", doc_root));
	hm->filepath.p = (char*) malloc((doc_root_len + 11) * sizeof(char));
	hm->filepath.len = doc_root_len + 11;
	memset(hm->filepath.p, 0, hm->filepath.len);
	strncpy(hm->filepath.p, doc_root, doc_root_len);
	strncat(hm->filepath.p, "/index.html", 11);

    } else {
	DBG(fprintf(stderr, "[INFO] PREVIOUS CONTENTS OF FILEPATH: %s %d\n", hm->filepath.p, (int) hm->filepath.len));
	DBG(fprintf(stderr, "[INFO] DOC ROOT: %s\n", doc_root));
	hm->filepath.p = (char*) malloc((doc_root_len + hm->uri.len) * sizeof(char));
	hm->filepath.len = doc_root_len + hm->uri.len;
	memset(hm->filepath.p, 0, hm->filepath.len);
	strncpy(hm->filepath.p, doc_root, doc_root_len);
	strncat(hm->filepath.p, hm->uri.p, hm->uri.len);
	//strncpy(hm->filepath.p + 1, hm->uri.p, hm->uri.len);
    }

    DBG(fprintf(stderr, "[INFO] http req filepath: %s\n", hm->filepath.p));

    // Check if valid file
    if ( fopen(hm->filepath.p, "r") == NULL) {
	if (errno == ENOENT) {
	    // File does not exist
	    create_http_response_msg(hm, 404, "Not Found");
	    return 0;
	} else if (errno == EACCES) {
	    // Permission denied
	    create_http_response_msg(hm, 403, "Forbidden");
	    return 0;
	}
    }

    // Check proto
    if (strncmp("HTTP/1.0", hm->proto.p, 8) != 0 && strncmp("HTTP/1.1", hm->proto.p, 8) != 0) {
	// Not HTTP/1.0 or HTTP/1.1
	create_http_response_msg(hm, 400, "Bad Request");
	return 0;
    }

    // For now ignoring any headers

    // Valid request if here
    create_http_response_msg(hm, 200, "OK");
    return 1;
}
