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

#define SERVERPORT "4950"

int main(int argc, char ** argv) {

    int socketh = 0;

    struct addrinfo hints = {0};
    struct addrinfo * serverinfo;
    struct addrinfo * current;

    int status = 0;
    int bytes_sent = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: talker <hostname> <message>\n");
        exit(1);
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1],
                         SERVERPORT,
                         &hints,
                         &serverinfo);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop over all returns and connect to the first possible.
    for (current=serverinfo; current != NULL; current = current -> ai_next) {
        // Get socket.
        socketh = socket(current->ai_family,
                         current->ai_socktype,
                         current->ai_protocol);

        if (socketh == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }

    if (current == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    bytes_sent = sendto(socketh,
                           argv[2],
                           strlen(argv[2]),
                           0,
                           current->ai_addr,
                           current->ai_addrlen);

    if (bytes_sent == -1) {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(serverinfo);

    printf("talker: sent %d bytes to %s\n", bytes_sent, argv[1]);
    close(socketh);

    return 0;
}
