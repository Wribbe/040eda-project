#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

#define PORT "3490"
#define BACKLOG 10

typedef struct thread_data_struct {
    int socket_descriptor;
    int done;
} thread_data_struct;

void * do_work(void * arg) {
    thread_data_struct * data = (thread_data_struct * )arg;
    int status = send(data->socket_descriptor,
                      "Hello world!",
                      13,
                      0);
    if (status == -1) {
        perror("send");
    }
    close(data->socket_descriptor);
    data->done = 1;
    return NULL;
}

void * get_in_address(struct sockaddr * sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in * )sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 * )sa)->sin6_addr);
}

int main(void) {

    int socket_descriptor = 0;
    int new_descriptor = 0;

    typedef struct addrinfo addrinfo;

    addrinfo hints = {0};
    addrinfo * server_info;
    addrinfo * current;

    struct sockaddr_storage their_address;
    socklen_t sin_size;

    int yes = 1;

    char string_buffer[INET6_ADDRSTRLEN];

    int return_value = 0;

    // Set up hints.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Use host ip.

    return_value = getaddrinfo(NULL, PORT, &hints, &server_info);
    if (return_value != 0) {
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(return_value));
        return 1;
    }

    // Loop through all the results and bind to the first one possible.
    current = server_info;
    while(current != NULL) {

        socket_descriptor = socket(current->ai_family,
                                   current->ai_socktype,
                                   current->ai_protocol);
        if (socket_descriptor == -1) {
            perror("server: socket");
            continue;
        }

        return_value = setsockopt(socket_descriptor,
                                  SOL_SOCKET,
                                  SO_REUSEADDR,
                                  &yes,
                                  sizeof(int));

        if (return_value == -1) {
            perror("setsockopt");
            exit(1);
        }

        return_value = bind(socket_descriptor,
                            current->ai_addr,
                            current->ai_addrlen);

        if (return_value == -1) {
            close(socket_descriptor);
            perror("server: bind");
            continue;
        }

        break;
    }

    // Free server_info data.
    freeaddrinfo(server_info);

    if (current == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(socket_descriptor, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections.\n");

    int num_threads = 100;
    int current_thread = 0;
    pthread_t thread_pool[num_threads];
    thread_data_struct thread_data[num_threads];

    for(;;) { // Main accept() loop.
        sin_size = sizeof their_address;
        new_descriptor = accept(socket_descriptor,
                                (struct sockaddr * )&their_address,
                                &sin_size);

        if (new_descriptor == -1) { // Error in accept()
            perror("accept");
            continue;
        }

        inet_ntop(their_address.ss_family,
                  get_in_address((struct sockaddr *)&their_address),
                  string_buffer,
                  sizeof string_buffer);

        printf("server: got connection from %s\n", string_buffer);

        thread_data[current_thread] = (thread_data_struct){new_descriptor, 0};
        pthread_create(&thread_pool[current_thread],
                       NULL,
                       do_work,
                       &thread_data[current_thread]);

        current_thread++;
        current_thread %= num_threads;
    }

    return 0;
}
