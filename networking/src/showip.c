#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char * argv[]) {

    struct addrinfo hints = {0};
    struct addrinfo * results;
    struct addrinfo * current;

    int status = 0;

    char ip_string[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "Usage: showip <hostname>\n");
        return 1;
    }

    hints.ai_family = AF_UNSPEC; // AF_INET / AF_INET6 to force v4/v6.
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets.

    if ((status = getaddrinfo(argv[1], NULL, &hints, &results)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    printf("IP adderss for %s:\n\n", argv[1]);

    for (current = results; current != NULL; current = current->ai_next) {
        void * address;
        const char * ip_version;

        // Get pointer to address, different for v4 and v6.
        if (current->ai_family == AF_INET) { // IPv4.
            struct sockaddr_in * ipv4 = (struct sockaddr_in * )current->ai_addr;
            address = &(ipv4->sin_addr);
            ip_version = "IPv4";
        } else { // IPv6.
            struct sockaddr_in6 * ipv6 = (struct sockaddr_in6 * )current->ai_addr;
            address = &(ipv6->sin6_addr);
            ip_version = "IPv6";
        }

        // Convert the IP to a string and print it.
        inet_ntop(current->ai_family, address, ip_string, sizeof(ip_string));
        printf(" %s: %s\n", ip_version, ip_string);
    }

    // Free the linked list.
    freeaddrinfo(results);

    return 0;
}
