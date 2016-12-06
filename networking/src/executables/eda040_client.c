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
//    // Connection complete.
//
//   // Send 101110.
//   uint32_t converted_number = string_to_bits("101");
//   printf("Sending: %" PRIu32 "\n", converted_number);
//   uint32_t send_data[] = {
//       0xff, // Reserved for the size.
//       htonl(converted_number),
//   };
//
//   // Set data size as first parameter.
//   send_data[0] = htonl(SIZE(send_data)-1);
//
//   printf("size of send_data: %zu\n", sizeof(send_data));
//
//   // Send data package.
//   int send_status = send(socket_descriptor,
//                          &send_data,
//                          sizeof(send_data),
//                          0);
//
//   // Check status of send.
//   if (send_status == -1) {
//       perror("send");
//   }
    uint32_t data_dump[4];
    int picture = 0;

    for (;;) {
 //   // Receive data.
        printf("Waiting for data.\n");
        numbytes = recv(socket_descriptor,
                        data_dump,
                        1+8+4,
                        0);

        if (numbytes == -1) {
            perror("recv");
            exit(1);
        }

        printf("Got data, num_bytes = %d.\n", numbytes);
        char * byte_pointer = (char * )data_dump;
        uint8_t flag = 0;
        memcpy(&flag, byte_pointer, sizeof(uint8_t));

        uint32_t time_top_unconverted = 0;
        memcpy(&time_top_unconverted, byte_pointer+1, sizeof(uint32_t));
        uint32_t time_top = ntohl(time_top_unconverted);

        uint32_t time_bottom_unconverted = 0;
        memcpy(&time_bottom_unconverted, byte_pointer+5, sizeof(uint32_t));
        uint32_t time_bottom = ntohl(time_bottom_unconverted);

        uint32_t picture_size_unconverted = 0;
        memcpy(&picture_size_unconverted, byte_pointer+9, sizeof(uint32_t));
        uint32_t picture_size = ntohl(picture_size_unconverted);

        printf("picture size: %" PRIu32 "\n", picture_size);

        unsigned char * picture_data = malloc(picture_size);
        unsigned char * picture_data_pointer = picture_data;

        uint32_t bytes = 0;


        while(bytes < picture_size) {
            int current_bytes = recv(socket_descriptor,
                                     picture_data_pointer,
                                     picture_size,
                                     0);
            if (current_bytes == -1) {
                fprintf(stderr, "Error, on receiving picture.\n");
                exit(1);
            }
            printf("current_bytes: %d\n", current_bytes);
            printf("Current bytes: %"PRIu32"!\n", bytes);
            bytes += current_bytes;
            picture_data_pointer += current_bytes;
        }
        printf("Got all the bytes: %"PRIu32"!\n", bytes);

        char filename_format[] = "data/images/CAMERA_OUTPUT_%04d.jpeg";
        char filepath[sizeof(filename_format)+24];
        sprintf(filepath, filename_format, picture);
        int written = printf("Saving image to disk: %s.\n", filepath);
        filepath[written] = '\0';
        FILE * output_file = fopen(filepath, "wb");
        fwrite(picture_data, picture_size, 1, output_file);
        fclose(output_file);

        picture += 1;

        free(picture_data);
        picture_data = NULL;
        picture_data_pointer = NULL;
    }

    close(socket_descriptor);

    return 0;
}
