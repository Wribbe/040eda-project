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

#define OUR_PORT "3490"
#define BACKLOG 10

#include <stddef.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include "capture.h"

#define TEN_TO_NINE 1000000000
#define TEN_TO_SIX 1000000
#define TIMESTAMP_POS 25
#define TIME_ARRAY_SIZE 8

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

void * receive_work_function(void * input_data)
{

    // Unpack input_data.
    struct socket_data * data = (struct socket_data * )input_data;

    socklen_t sin_size;
    printf("server: waiting for connections on port: %s.\n", OUR_PORT);

    int new_socket_descriptor = 0;

    for(;;) { // Main accept() loop.

        sin_size = data->sizeof_their_address;
        new_socket_descriptor = accept(*data->socket_descriptor,
                                (struct sockaddr * )data->their_address,
                                &sin_size);

        if (new_socket_descriptor == -1) { // Error in accept()
            perror("accept");
            continue;
        }

        inet_ntop(data->their_address->ss_family,
                  get_in_address((struct sockaddr *)data->their_address),
                  data->string_buffer,
                  data->sizeof_string_buffer);

//        printf("server: got connection from %s\n", data->string_buffer);
//
//        char message[] = "HELLO CLIENT!";
//        int status = send(new_socket_descriptor, message, sizeof(message), 0);
//        if (status == -1) {
//            perror("send");
//        }
    }
}

//void * send_work_function(void * input_data)
//{
//
//    socklen_t sin_size;
//    printf("server: waiting to send on port: %s.\n", OUR_PORT);
//
//    for(;;) { // Main accept() loop.
//
//        sin_size = sizeof data.their_address;
//        new_socket_descriptor = accept(*data.socket_descriptor,
//                                (struct sockaddr * )data.their_address,
//                                &sin_size);
//
//        if (new_socket_descriptor == -1) { // Error in accept()
//            perror("accept");
//            continue;
//        }
//
//        inet_ntop(data.their_address->ss_family,
//                  get_in_address((struct sockaddr *)data.their_address),
//                  data.string_buffer,
//                  sizeof data.string_buffer);
//
//        printf("server: got connection from %s\n", data.string_buffer);
//
//        char message[] = "HELLO CLIENT!";
//        int status = send(new_socket_descriptor, message, sizeof(message), 0);
//        if (status == -1) {
//            perror("send");
//        }
//    }
//}


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

//    pthread_t send = {0};
//    pthread_create(&send, NULL, send_work_function, NULL);
    pthread_join(receive, NULL);

//    pthread_join(send, NULL);
}
