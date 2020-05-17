#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>

#include "net.h"
#include "str.h"
#include "http.h"
#include "debug.h"

#define BACKLOG 10

// recv and send buffers
#define RECVBUF_SIZE 4096
#define SENDBUF_SIZE 4096
unsigned char recvbuf[RECVBUF_SIZE];
unsigned char sendbuf[SENDBUF_SIZE];

// Default port and document root for web server
// Override with -port and -document_root cli options
int port = 8000;
char *document_root = "/home/chris/web_server/scu.edu";

// FD_SETSIZE is 1024 (the limit for # of fds select tracks)
http_message_t* global_state[FD_SETSIZE];

// Callback functions return this struct denoting the status
// of a file descriptor. If both are false, the connection
// can be closed.
typedef struct {
    int read;
    int write;
} fd_status_t;

// Some shorthands
const fd_status_t fd_status_R = {.read = 1, .write = 0};
const fd_status_t fd_status_W = {.read = 0, .write = 1};
const fd_status_t fd_status_RW = {.read = 1, .write = 1};
const fd_status_t fd_status_NORW = {.read = 0, .write = 0};

// Callback for when a new client connection is made
fd_status_t cb_client_connect(int client_fd, struct sockaddr_in * client_addr, socklen_t addrlen)
{
    // Print out ip of client
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(client_addr->sin_family, &(client_addr->sin_addr), ip, sizeof ip);
    DBG(fprintf(stderr, "[INFO] new connection from %s\n", ip));
    DBG(fprintf(stderr, "[INFO] new fd: %d\n", client_fd));

    // Ready to read
    return fd_status_R;
}

// Callback for read events.
// Because this server only supports HTTP/1.0 this callback
// will be invoked when a new http request is received.
fd_status_t cb_client_read(int client_fd)
{
    memset(recvbuf, 0, RECVBUF_SIZE);

    int bytes_read = recv(client_fd, recvbuf, RECVBUF_SIZE, 0);
    if (bytes_read == 0) {
	printf("[INFO] client socket %d disconnected\n", client_fd);
	return fd_status_NORW;
    } else if (bytes_read < 0) {
	// Not really ready for a recv operation yet
	// This can sometimes occur with non-blocking sockets
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    return fd_status_R;
	} else {
	    perror("[ERROR] recv()");
	    exit(EXIT_FAILURE);
	}
    }

    DBG(fprintf(stderr, "[INFO] recv msg (%d bytes): \n%s\n", bytes_read, recvbuf));

    // Create new http msg for this connection
    http_message_t* hm = (http_message_t*) malloc(sizeof(http_message_t));
    initialize_http_message(hm);
    // Add memory block for raw http message
    hm->message.p = (char*) malloc(bytes_read * sizeof(char));
    hm->message.len = bytes_read;
    //memset(hm->message.p, 0, bytes_read);
    strncpy(hm->message.p, recvbuf, bytes_read);

    // Add this http message to the global state for this connection
    global_state[client_fd] = hm;
    return fd_status_W;
}

// Callback for write events
fd_status_t cb_client_write(int client_fd)
{
    fd_status_t ret;
    int bytes_sent, valid_req;

    http_message_t *hm = global_state[client_fd];

    DBG(fprintf(stderr, "[INFO] cb_client_write() - first_resp = %d\n", hm->first_resp));
    memset(sendbuf, 0, SENDBUF_SIZE);
    if (hm->first_resp == 1) {
	// first response to client
	valid_req = parse_http_request(hm, document_root);
	hm->first_resp = 0;
	DBG(fprintf(stderr, "[INFO] first response for fd %d\n", client_fd));
	if (!valid_req) {
	    // send error msg
	    snprintf((char *) sendbuf, sizeof(sendbuf), "HTTP/1.0 %d %.*s\r\n\r\n",
			hm->resp_code, (int) hm->resp_status_msg.len, hm->resp_status_msg.p);
	    bytes_sent = send(client_fd, (char *) sendbuf, strlen((char *) sendbuf), 0);
	    if (bytes_sent == -1) {
		perror("[ERROR] send()");
		exit(EXIT_FAILURE);
	    }
	    free_http_message(hm);
	    global_state[client_fd] = NULL;
	    ret = fd_status_NORW;
	} else {
	    // send first message informing client that content is coming
	    // get content length
	    struct stat st;
	    stat(hm->filepath.p, &st);
	    // get content type (look at file extension)
	    const char *content_type;
	    char *ext = strrchr(hm->filepath.p + 1, '.');
	    if (!ext) {
		DBG(fprintf(stderr, "[INFO] no file extension... defaulting to text/plain\n"));
		    content_type = content_types[TEXT];
	    } else if (strcmp(".txt", ext) == 0)
		content_type = content_types[TEXT];
	    else if (strcmp(".html", ext) == 0)
		content_type = content_types[HTML];
	    else if (strcmp(".css", ext) == 0)
		content_type = content_types[CSS];
	    else if (strcmp(".png", ext) == 0)
		content_type = content_types[PNG];
	    else if (strcmp(".jpeg", ext) == 0 || strcmp(".jpg", ext) == 0)
		content_type = content_types[JPEG];
	    else if (strcmp(".gif", ext) == 0)
		content_type = content_types[GIF];
	    else {
		DBG(fprintf(stderr, "[INFO] file type not supported... defaulting to text/plain\n"));
		content_type = content_types[TEXT];
	    }

	    // Create DATE header
	    char date[50];
	    struct tm* gmt;
	    time_t now;
	    time(&now);
	    gmt = gmtime(&now);
	    strftime(date, sizeof(date), "%a, %d %h %Y %H:%M:%S GMT", gmt);
	    DBG(fprintf(stderr, "[INFO] DATE: %s\n", date));
	    snprintf((char *) sendbuf, sizeof(sendbuf),
		     "HTTP/1.0 %d %.*s\r\nDate: %s\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n",
		     hm->resp_code, (int) hm->resp_status_msg.len, hm->resp_status_msg.p,
		     date, st.st_size, content_type);
	    DBG(fprintf(stderr, "[INFO] PRINTING OUT MSG:\n%s", sendbuf));

	    bytes_sent = send(client_fd, (char *) sendbuf, strlen((char *) sendbuf), 0);
	    if (bytes_sent == -1) {
		perror("[ERROR] send()");
		exit(EXIT_FAILURE);
	    }
	    // still need to send the resource requested
	    ret = fd_status_W;
	}
    } else {
	if (hm->fp == NULL) {
	    // file isnt open yet
	    DBG(fprintf(stderr, "[INFO] opening file requested: %s\n", hm->filepath.p));
	    hm->fp = fopen(hm->filepath.p, "rb");
	}

	if (feof(hm->fp)) {
	    // end of file reached
	    // send last msg in http response
	    DBG(fprintf(stderr, (char *) sendbuf, sizeof(sendbuf), "\r\n\r\n"));
	    bytes_sent = send(client_fd, (char *) sendbuf, strlen((char *) sendbuf), 0);
	    if (bytes_sent == -1) {
		perror("[ERROR] send()");
		exit(EXIT_FAILURE);
	    }
	    free_http_message(global_state[client_fd]);
	    global_state[client_fd] = NULL;
	    ret = fd_status_NORW;
	} else{
	    int numread = fread(sendbuf, sizeof(unsigned char), SENDBUF_SIZE, hm->fp);
	    if (numread == -1) {
		perror("[ERROR] read()");
		exit(EXIT_FAILURE);
	    } else if (numread == 0) {
		// This shouldn't happen
		// send last msg in http response
		DBG(fprintf(stderr, (char *) sendbuf, sizeof(sendbuf), "\r\n\r\n"));
		bytes_sent = send(client_fd, (char *) sendbuf, strlen((char *) sendbuf), 0);
		if (bytes_sent == -1) {
		    perror("[ERROR] send()");
		    exit(EXIT_FAILURE);
		}
		free_http_message(global_state[client_fd]);
		global_state[client_fd] = NULL;
		ret = fd_status_NORW;
	    } else {
		// send next block of file
		bytes_sent = send(client_fd, (char *) sendbuf, SENDBUF_SIZE, 0);
		if (bytes_sent == -1) {
		    perror("[ERROR] send()");
		    exit(EXIT_FAILURE);
		}
		ret = fd_status_W;
	    }

	}
    }

    DBG(fprintf(stderr, "[INFO] sent msg (%d bytes)\n", bytes_sent));
    return ret;
}


void parse_cli_options(int argc, char **argv)
{
    while(1) {
	static struct option long_options[] =
	    {
		{"port", required_argument, NULL, 0},
		{"document_root", required_argument, NULL, 0},
		{0,0,0,0}
	    };

	int options_index = 0;

	int c;
	c = getopt_long_only(argc, argv, "", long_options, &options_index);

	if (c == -1)
	    break;

	switch (c) {
	case 0:
	    if (optarg && options_index == 0) {
		port = atoi(optarg);
	    } else if (optarg && options_index == 1) {
		// strip off '/' at end of path
		if (optarg[strlen(optarg)-1] == '/')
		    optarg[strlen(optarg)-1] = 0;
		document_root = optarg;
	    }
	    break;
	default:
	    exit(EXIT_FAILURE);
	}
    }
}

int main(int argc, char *argv[])
{
    parse_cli_options(argc, argv);
    DBG(fprintf(stderr, "[INFO] Document root: %s\n", document_root));

    int server_fd;
    struct sockaddr_in server_addr;
    int addrlen = sizeof(server_addr);

    memset(&server_addr, '0', sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Create server listening socket
    if ( (server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
	perror("[ERROR] socket()");
	exit(EXIT_FAILURE);
    }

    // Bind to server listening socket
    if ( bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
	perror("[ERROR] bind()");\
	exit(EXIT_FAILURE);
    }

    // Start listening for client connections
    if ( listen(server_fd, BACKLOG) == -1) {
	perror("[ERROR] listen()");
	exit(EXIT_FAILURE);
    }

    // Make socket non-blocking
    set_socket_non_blocking(server_fd);

    DBG(fprintf(stderr, "[INFO] server listening on port %d\n", 8000));

    // Read and write FD sets that will be tracked in the loop
    fd_set readfds_active, writefds_active;
    FD_ZERO(&readfds_active);
    FD_ZERO(&writefds_active);

    // Register server listening socket. Monitor for read events (new client connections)
    FD_SET(server_fd, &readfds_active);

    // Keep track of max fd in the fdset. Prevents having to iterate to
    // FD_SETSIZE on every call of select()
    int fdset_max = server_fd;


    // After select is called, the below two fd sets will be set with the fd that are
    // read to be read from/written to
    fd_set readfds, writefds;
    int i, nready;

    while (1) {
	fd_set readfds = readfds_active;
	fd_set writefds = writefds_active;

	if ( (nready = select(fdset_max + 1, &readfds, &writefds, NULL, NULL)) == -1) {
	    perror("[ERROR] select()");
	    exit(EXIT_FAILURE);
	}
	DBG(fprintf(stderr, "[INFO] nready file descriptors: %d\n", nready));

	for (i = 0; i <= fdset_max && nready > 0; ++i) {
	    if (FD_ISSET(i, &readfds)) {
		DBG(fprintf(stderr, "[INFO] READ I/O for fd %d\n", i));
		nready--;
		if (i == server_fd) {
		    int client_fd;
		    struct sockaddr_in client_addr;
		    memset(&client_addr, '0', sizeof(client_addr));
		    // Accept client connection
		    if ( (client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen)) == -1) {
			perror("[ERROR] accept()");
			exit(EXIT_FAILURE);
		    }

		    // Make socket non-blocking
		    set_socket_non_blocking(client_fd);

		    if (client_fd > fdset_max) fdset_max = client_fd;

		    fd_status_t status = cb_client_connect(client_fd, (struct sockaddr_in *) &client_addr, (socklen_t) addrlen);

		    if (status.read)
			FD_SET(client_fd, &readfds_active);
		    else
			FD_CLR(client_fd, &readfds_active);
		    if (status.write)
			FD_SET(client_fd, &writefds_active);
		    else
			FD_CLR(client_fd, &writefds_active);

		} else {
		    fd_status_t status = cb_client_read(i);
		    if (status.read)
			FD_SET(i, &readfds_active);
		    else
			FD_CLR(i, &readfds_active);
		    if (status.write)
			FD_SET(i, &writefds_active);
		    else
			FD_CLR(i, &writefds_active);

		    if (!status.read && !status.write) {
			DBG(fprintf(stderr, "[INFO] closing connection to fd %d\n", i));
			close(i);
		    }
		}
	    }

	    if (FD_ISSET(i, &writefds)) {
		DBG(fprintf(stderr, "[INFO] WRITE I/O for fd %d\n", i));
		nready--;
		fd_status_t status = cb_client_write(i);
		if (status.read)
		    FD_SET(i, &readfds_active);
		else
		    FD_CLR(i, &readfds_active);
		if (status.write)
		    FD_SET(i, &writefds_active);
		else
		    FD_CLR(i, &writefds_active);

		if (!status.read && !status.write) {
		    DBG(fprintf(stderr, "[INFO] closing connection to fd %d\n", i));
		    close(i);
		}
	    }
	}
    }
}
