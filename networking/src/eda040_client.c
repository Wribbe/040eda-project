#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT "3490"

#include "net_data.h"

void * get_in_address(struct sockaddr * sa) {
    void * return_pointer;
    if (sa->sa_family == AF_INET) {
        return_pointer = &(((struct sockaddr_in * )sa)->sin_addr);
    } else {
        return_pointer = &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
    return return_pointer;
}

int main(int argc, char * argv[]) {

    int socket_descriptor = 0;
    int numbytes = 0;
    char data_buffer[MAX_DATA_SIZE];

    struct addrinfo hints = {0};
    struct addrinfo * serverinfo;
    struct addrinfo * current;

    int status = 0;

    char address_buffer[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "usage: client <hostname>\n");
        exit(1);
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(argv[1],
                         PORT,
                         &hints,
                         &serverinfo);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all the results and connect to the first possible.
    current = serverinfo;
    for(current = serverinfo; current != NULL; current = current->ai_next) {
        socket_descriptor = socket(current->ai_family,
                                   current->ai_socktype,
                                   current->ai_protocol);

        if (socket_descriptor == -1) {
            perror("client: socket");
            continue;
        }

        status = connect(socket_descriptor,
                         current->ai_addr,
                         current->ai_addrlen);

        if (status == -1) {
            close(socket_descriptor);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (current == NULL) {
        fprintf(stderr, "client: failed to connect.\n");
        return 2;
    }

    inet_ntop(current->ai_family,
              get_in_address((struct sockaddr * )current->ai_addr),
              address_buffer,
              sizeof address_buffer);
    printf("Connecting to: %s\n", address_buffer);

    freeaddrinfo(serverinfo); // All done with this struct.

    numbytes = recv(socket_descriptor,
                    data_buffer,
                    MAX_DATA_SIZE-1,
                    0);

    if (numbytes == -1) {
        perror("recv");
        exit(1);
    }

    struct data_packet * received_data = (struct data_packet * )data_buffer;

    printf("Client received: %s\n", received_data->message);

    close(socket_descriptor);

    return 0;
}
