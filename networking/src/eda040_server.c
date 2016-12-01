#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // close()
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <inttypes.h>

#define OUR_PORT "3490"
#define BACKLOG 10

#include <stddef.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include "capture.h"
#include "net_data.h"

#define TEN_TO_NINE 1000000000
#define TEN_TO_SIX 1000000
#define TIMESTAMP_POS 25
#define TIME_ARRAY_SIZE 8

#define SIZE(x) sizeof(x)/sizeof(x[0])

void * get_in_address(struct sockaddr * sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in * )sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 * )sa)->sin6_addr);
}


struct socket_data {
    int * socket_descriptor;
    struct addrinfo hints;
    struct sockaddr_storage * their_address;
    size_t sizeof_their_address;
    struct addrinfo * current;
    char string_buffer[INET6_ADDRSTRLEN];
    size_t sizeof_string_buffer;
    int new_descriptor;
};

struct data_node
{
    uint32_t size;
    uint32_t * data;
};

void open_socket(const char * PORT, struct socket_data * socket_data) {

    int yes = 1;

    // Set up hints.
    socket_data->hints.ai_family = AF_UNSPEC;
    socket_data->hints.ai_socktype = SOCK_STREAM;
    socket_data->hints.ai_flags = AI_PASSIVE; // Use host ip.

    struct addrinfo * server_info = NULL;

    int return_value = getaddrinfo(NULL, PORT, &socket_data->hints, &server_info);
    if (return_value != 0) {
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(return_value));
        exit(1);
    }

    // Loop through all the results and bind to the first one possible.
    socket_data->current = server_info;
    while(socket_data->current != NULL) {

        *(socket_data->socket_descriptor) = socket(socket_data->current->ai_family,
                                                   socket_data->current->ai_socktype,
                                                   socket_data->current->ai_protocol);

        if (*socket_data->socket_descriptor == -1) {
            perror("server: socket");
            continue;
        }

        return_value = setsockopt(*socket_data->socket_descriptor,
                                  SOL_SOCKET,
                                  SO_REUSEADDR,
                                  &yes,
                                  sizeof(int));

        if (return_value == -1) {
            perror("setsockopt");
            exit(1);
        }

        return_value = bind(*socket_data->socket_descriptor,
                            socket_data->current->ai_addr,
                            socket_data->current->ai_addrlen);

        if (return_value == -1) {
            close(*socket_data->socket_descriptor);
            perror("server: bind");
            continue;
        }

        break;
    }

    // Free server_info data.
    freeaddrinfo(server_info);

    if (socket_data->current == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(*socket_data->socket_descriptor, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

}

struct data_node * receive_list = NULL;
struct data_node * send_list = NULL;

int fake_lock = 1;

void * receive_work_function (void * input_data)
{
    // Unpack input_data.
    struct socket_data * data = (struct socket_data * )input_data;

    socklen_t sin_size;
    printf("server: waiting for data on port: %s.\n", OUR_PORT);

    int new_socket_descriptor = 0;
    int numbytes = 0;

    for(;;) { // Main accept() loop.

        sin_size = data->sizeof_their_address;
        new_socket_descriptor = accept(*data->socket_descriptor,
                                (struct sockaddr * )data->their_address,
                                &sin_size);

        if (new_socket_descriptor == -1) { // Error in accept()
            perror("accept");
            continue;
        }

        // Store descriptor for use in the other thread.
        printf("socket: %d\n", new_socket_descriptor);
        ((struct socket_data *)input_data)->new_descriptor = new_socket_descriptor;

        inet_ntop(data->their_address->ss_family,
                  get_in_address((struct sockaddr *)data->their_address),
                  data->string_buffer,
                  data->sizeof_string_buffer);
        printf("server: got data from %s\n", data->string_buffer);

        fake_lock = 1;

        uint32_t data_buffer[MAX_DATA_SIZE];

        numbytes = recv(new_socket_descriptor,
                        data_buffer,
                        MAX_DATA_SIZE,
                        0);

        if (numbytes == -1) {
            perror("recv");
            exit(1);
        }

        uint32_t * data_pointer = (uint32_t * )data_buffer;
        uint32_t data_size = ntohl(*data_pointer);

        uint32_t * data = malloc(sizeof(uint32_t)*data_size);
        // Convert received data.
        for (uint32_t i = 0; i < data_size; i++) {
            data[i] = ntohl(data_pointer[1+i]);
        }

        for (uint32_t i = 0; i < data_size-1; i++) {
            printf("received data [%" PRIu32 "] = %" PRIu32 ".\n", i, data[i]);
        }

        struct data_node * node = malloc(sizeof(struct data_node));
        node->size = data_size;
        node->data = data;

        send_list = node;

        fake_lock = 0;
    }
}

void * send_work_function (void * input_data)
{
    // Unpack input_data.
    struct socket_data * data = (struct socket_data * )input_data;

    for(;;) { // waiting for data loop.

        if (!fake_lock && send_list != NULL) {

            // Use socket descriptor from receive thread.
            int new_socket_descriptor = data->new_descriptor;

            struct data_node * current = send_list;

            uint32_t data_size = current->size;
            uint32_t send_data[data_size+1]; // Leave room for length in front.

            uint32_t * data_to_be_sent = current->data;
            printf("data_to_bes_sent[0]: %" PRIu32 ".\n", data_to_be_sent[0]);
            // Add size to send_data.
            send_data[0] = htonl(data_size);
            // Add rest of data to send_data.
            for (uint32_t i = 0; i < data_size; i++) {
                send_data[i+1] = htonl(data_to_be_sent[i]);
                printf("Packing data: %" PRIu32 " as :%" PRIu32 ".\n", data_to_be_sent[i], send_data[i+1]);
            }
            // Send data.
            printf("Sending data of data_size: %" PRIu32 ".\n", data_size+1);
            printf("data_size in storage: %" PRIu32 " converted: %" PRIu32 ".\n", send_data[0], ntohl(send_data[0]));
            int status = send(new_socket_descriptor, send_data, (data_size+1)*sizeof(uint32_t), 0);
            if (status == -1) {
                perror("send");
            }

            fake_lock = 1;
        }
    }
}

int main(void)
{
    // Create socket;
    int socket_descriptor = 0;

    struct socket_data data = {0};
    struct sockaddr_storage their_address;

    data.their_address = &their_address;
    data.sizeof_their_address = sizeof(data.their_address);
    data.sizeof_string_buffer = sizeof(data.string_buffer);
    data.socket_descriptor = &socket_descriptor;
    open_socket(OUR_PORT, &data);

    pthread_t receive = {0};
    pthread_create(&receive, NULL, receive_work_function, &data);

    pthread_t send = {0};
    pthread_create(&send, NULL, send_work_function, &data);
    pthread_join(receive, NULL);
    pthread_join(send, NULL);
}
