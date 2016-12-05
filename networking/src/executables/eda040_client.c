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
#include <netinet/in.h>
#include <inttypes.h>

#define PORT "3490"

#include "net_data.h"

#define SIZE(x) sizeof(x)/sizeof(x[0])

void * get_in_address(struct sockaddr * sa) {
    void * return_pointer;
    if (sa->sa_family == AF_INET) {
        return_pointer = &(((struct sockaddr_in * )sa)->sin_addr);
    } else {
        return_pointer = &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
    return return_pointer;
}

uint32_t string_to_bits(const char * string) {
    // Convert string to bit pattern in a uint32_t value.
    const char * c = string;
    uint32_t result = 0;
    size_t loops = 0;
    size_t max_loops = sizeof(uint32_t)*8;
    while (*c != '\0') {
        if (loops > max_loops) { // Don't exceed bit size.
            break;
        }
        if (loops > 0) {
            result <<= 1;
        }
        switch(*c) {
            case '1': result += 1; break;
            default: break;
        }
        loops++;
        // Advance pointer.
        c++;
    }
    return result;
}

int main(int argc, char * argv[]) {

    int socket_descriptor = 0;
    int numbytes = 0;
    uint32_t data_buffer[MAX_DATA_SIZE];

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
    // Connection complete.

 //   // Send 101110.
 //   uint32_t converted_number = string_to_bits("101");
 //   printf("Sending: %" PRIu32 "\n", converted_number);
 //   uint32_t send_data[] = {
 //       0xff, // Reserved for the size.
 //       htonl(converted_number),
 //   };

 //   // Set data size as first parameter.
 //   send_data[0] = htonl(SIZE(send_data)-1);

 //   printf("size of send_data: %zu\n", sizeof(send_data));

 //   // Send data package.
 //   int send_status = send(socket_descriptor,
 //                          &send_data,
 //                          sizeof(send_data),
 //                          0);

 //   // Check status of send.
 //   if (send_status == -1) {
 //       perror("send");
 //   }

 //   // Receive data.
    printf("Waiting for data.\n");
    numbytes = recv(socket_descriptor,
                    data_buffer,
                    sizeof(uint32_t)*2,
                    0);

    if (numbytes == -1) {
        perror("recv");
        exit(1);
    }

    printf("Got data, num_bytes = %d.\n", numbytes);

    uint32_t * message = (uint32_t * )data_buffer;
    uint32_t message_size = ntohl(*message);
    uint32_t original_data_size = ntohl(*(message+1));

    uint32_t * re_converted_data = malloc(message_size);

    printf("Size of message: %" PRIu32 "\n", message_size);
    printf("Size of original data: %" PRIu32 "\n", original_data_size);

    unsigned char * byte_pointer = (unsigned char * )re_converted_data;
    while ((uint32_t)numbytes < message_size && numbytes != -1) {
        int current_bytes = recv(socket_descriptor,
                                 byte_pointer,
                                 message_size,
                                 0);
        if (current_bytes == -1) {
            fprintf(stderr, "Error, on receiving picture.\n");
            exit(1);
        }
        byte_pointer += current_bytes;
        numbytes += current_bytes;
    }
    printf("Received a total of %d bytes.", numbytes);

    // Convert and print the data to file.
    for (uint32_t i=0; i<message_size/sizeof(uint32_t); i++) {
        uint32_t raw_data = re_converted_data[i];
        uint32_t converted_data = ntohl(raw_data);
        re_converted_data[i] = converted_data;
    }

    FILE * output_file = fopen("captured_image.jpeg", "wb");
    fwrite(re_converted_data, original_data_size, 1, output_file);
    fclose(output_file);

    free(re_converted_data);

    for (;;) {
    }
    close(socket_descriptor);

    return 0;
}
