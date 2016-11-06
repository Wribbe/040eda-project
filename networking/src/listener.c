#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MYPORT "4950"
#define MAX_BUFFLEN 100

void * get_in_address(struct sockaddr * sa) {
    void * return_pointer;
    if (sa->sa_family == AF_INET) { // IPv4.
        return_pointer = &(((struct sockaddr_in * )sa)->sin_addr);
    } else { // IPv6.
        return_pointer = &(((struct sockaddr_in6 * )sa)->sin6_addr);
    }
    return return_pointer;
}

int main(void) {

    int desc_socket = 0;

    struct addrinfo hints = {0};
    struct addrinfo * serverinfo;
    struct addrinfo * current;

    int status = 0;
    int num_bytes = 0;

    struct sockaddr_storage their_address = {0};

    char data_buffer[MAX_BUFFLEN];
    char address_buffer[INET6_ADDRSTRLEN];

    socklen_t address_length;

    // Set up hints.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // Use host IP.

    status = getaddrinfo(NULL,
                         MYPORT,
                         &hints,
                         &serverinfo);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all entries in serverinfo.
    for (current=serverinfo; current != NULL; current = current->ai_next) {
        // Retrieve socket.
        desc_socket = socket(current->ai_family,
                             current->ai_socktype,
                             current->ai_protocol);
        if (desc_socket == -1) {
            perror("listener: socket");
            continue;
        }

        // Bind current socket to correct port.
        status = bind(desc_socket,
                      current->ai_addr,
                      current->ai_addrlen);

        // Check status of bind.
        if (status == -1) {
            close(desc_socket);
            perror("listener: bind");
            continue;
        }

        break; // Found good entry to connect to.
    }

    if (current == NULL) { // No valid connection.
        fprintf(stderr, "listener: filed to bind socket\n");
        return 2;
    }

    freeaddrinfo(serverinfo); // Free serverinfo struct.

    printf("listener: waiting to recvfrom..\n");

    address_length = sizeof their_address;

    struct sockaddr * their_cast = (struct sockaddr * )&their_address;

    num_bytes = recvfrom(desc_socket,
                         data_buffer,
                         MAX_BUFFLEN-1,
                         0,
                         their_cast,
                         &address_length);

    if (num_bytes == -1) {
        perror("recvfrom");
        exit(1);
    }

    const char * sender = inet_ntop(their_address.ss_family,
                                    get_in_address(their_cast),
                                    address_buffer,
                                    sizeof address_buffer);

    printf("listener: got packet from %s\n", sender);
    printf("listener: package is %d bytes long\n", num_bytes);
    data_buffer[num_bytes] = '\0';
    printf("listener: packet contains \"%s\"\n", data_buffer);

    // Close socket.
    close(desc_socket);

    return 0;
}
