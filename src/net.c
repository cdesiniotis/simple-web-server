#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

void set_socket_non_blocking(int fd)
{
    // Get the file status flags
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
	perror("[ERROR] getting flags with fcntl()");
	exit(EXIT_FAILURE);
    }

    // Set the file status flags to include non blocking
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
	perror("[ERROR] setting flags with fcntl()");
	exit(EXIT_FAILURE);
    }
}
