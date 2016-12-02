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
    int * new_socket_descriptor;
    struct sockaddr_storage * their_address;
    size_t sizeof_their_address;
    char string_buffer[INET6_ADDRSTRLEN];
    size_t sizeof_string_buffer;
    pthread_cond_t * data_to_send_sig;
    pthread_cond_t * data_to_receive_sig;
    pthread_mutex_t * data_mutex;
};

struct data_node
{
    uint32_t size;
    uint32_t * data;
};

void open_socket(const char * PORT, struct socket_data * socket_data) {

    int yes = 1;

    struct addrinfo hints = {0};
    struct addrinfo * server_info = NULL;

    // Set up hints.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Use host ip.


    int return_value = getaddrinfo(NULL, PORT, &hints, &server_info);
    if (return_value != 0) {
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(return_value));
        exit(1);
    }

    // Loop through all the results and bind to the first one possible.
    while(server_info != NULL) {

        *(socket_data->socket_descriptor) = socket(server_info->ai_family,
                                                   server_info->ai_socktype,
                                                   server_info->ai_protocol);

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
                             server_info->ai_addr,
                             server_info->ai_addrlen);

        if (return_value == -1) {
            close(*socket_data->socket_descriptor);
            perror("server: bind");
            continue;
        }

        break;
    }

    // Free server_info data.
    freeaddrinfo(server_info);

    if (server_info == NULL) {
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

    int new_socket_descriptor = *data->new_socket_descriptor;
    int numbytes = 0;

    for(;;) { // Main accept() loop.

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
            int new_socket_descriptor = *data->new_socket_descriptor;

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

int accept_on_socket(struct socket_data * data)
{
    /* Run accept on the data in struct socket_data, return the new socket
     * descriptor.
     * */
    socklen_t sin_size = data->sizeof_their_address;
    int new_socket_descriptor = accept(*data->socket_descriptor,
                                       (struct sockaddr * )data->their_address,
                                       &sin_size);
    return new_socket_descriptor;
}

int main(void)
{
    // Create socket;
    int socket_descriptor = 0;

    struct socket_data data = {0};
    struct sockaddr_storage their_address;

    // Create data lock.
    pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Create signals.
    pthread_cond_t data_to_send_sig = PTHREAD_COND_INITIALIZER;
    pthread_cond_t data_to_receive_sig = PTHREAD_COND_INITIALIZER;

    // Populate the socket_data struct with concurrency objects.
    data.data_to_send_sig = &data_to_send_sig;
    data.data_to_send_sig = &data_to_receive_sig;
    data.data_mutex = &data_mutex;

    // Populate the socket_data with data related to socket communication.
    data.their_address = &their_address;
    data.sizeof_their_address = sizeof(data.their_address);
    data.sizeof_string_buffer = sizeof(data.string_buffer);
    data.socket_descriptor = &socket_descriptor;
    open_socket(OUR_PORT, &data);

    // Accept and get a new connection.
    printf("server: waiting for data on port: %s.\n", OUR_PORT);

    int new_socket_descriptor = accept_on_socket(&data);
    while (new_socket_descriptor == -1) {
        perror("accept");
        new_socket_descriptor = accept_on_socket(&data);
    }

    // Populate data with new socket.
    data.new_socket_descriptor = &new_socket_descriptor;

    // Create and run receive thread, populates data necessary for send thread.
    pthread_t receive = {0};
    pthread_create(&receive, NULL, receive_work_function, &data);

    // Create and run send thread.
    pthread_t send = {0};
    pthread_create(&send, NULL, send_work_function, &data);

    // Join threads.
    pthread_join(receive, NULL);
    pthread_join(send, NULL);
}
