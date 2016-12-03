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
    pthread_cond_t * data_to_send_sig;
    pthread_cond_t * data_was_received_sig;
    pthread_mutex_t * data_mutex;
};

struct data_node
{
    uint32_t size;
    uint32_t * data;
    struct data_node * next;
};

struct data_node * receive_list = NULL;
struct data_node * receive_last = NULL;
struct data_node * send_list = NULL;
struct data_node * send_last = NULL;

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

void * receive_work_function (void * input_data)
{
    // Unpack input_data.
    struct socket_data * thread_data = (struct socket_data * )input_data;
    int numbytes = 0;
    int new_socket_descriptor = 0;
    int temp_socket_descriptor = 0;
    int socket_descriptor = 0;
    char client_address_buffer[INET6_ADDRSTRLEN];

    struct sockaddr_storage their_address = {0};

    const char * output_tag = "[RECEIVE]:";

    // Get the values from shared data.
    pthread_mutex_lock(thread_data->data_mutex);

    socket_descriptor = *(thread_data->socket_descriptor);
    // Copy their address locally.
    memcpy(&their_address,
           thread_data->their_address,
           thread_data->sizeof_their_address);

    pthread_mutex_unlock(thread_data->data_mutex);

    for(;;) { // Main accept() loop.

        // Accept and get a new connection.
        printf("%s waiting for data on port: %s.\n", output_tag, OUR_PORT);

        // This is a blocking call.
        socklen_t sin_size = sizeof(their_address);
        temp_socket_descriptor= accept(socket_descriptor,
                                       (struct sockaddr * )&their_address,
                                       &sin_size);

        // Populate shared data with new socket descriptor.
        pthread_mutex_lock(thread_data->data_mutex);
        new_socket_descriptor = temp_socket_descriptor;
        thread_data->new_socket_descriptor = &new_socket_descriptor;
        pthread_mutex_unlock(thread_data->data_mutex);

        inet_ntop(their_address.ss_family,
                  get_in_address((struct sockaddr *)&their_address),
                  client_address_buffer,
                  sizeof(client_address_buffer));
        printf("%s got data from %s\n", output_tag, client_address_buffer);

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

        uint32_t * converted_data = malloc(sizeof(uint32_t)*data_size);
        // Convert received data.
        for (uint32_t i = 0; i < data_size; i++) {
            converted_data[i] = ntohl(data_pointer[1+i]);
        }

        // Information about data.
        for (uint32_t i = 0; i < data_size-1; i++) {
            printf("%s Received data [%" PRIu32 "] = %" PRIu32 ".\n",
                   output_tag,
                   i,
                   converted_data[i]);
        }

        // Create new node for the receive list.
        struct data_node * node = malloc(sizeof(struct data_node));

        // Populate the node.
        node->size = data_size;
        node->data = converted_data;
        node->next = NULL;

        // Lock data_mutex.
        pthread_mutex_lock(thread_data->data_mutex);

        // Make new list or append.
        if (receive_list == NULL) { // New node.
            receive_list = node;
            receive_last = node;
        } else { // Append to last.
            receive_last->next = node;
            receive_last = receive_last->next;
        }

        // Signal that data was received.
        pthread_cond_signal(thread_data->data_was_received_sig);

        // Unlock the mutex.
        pthread_mutex_unlock(thread_data->data_mutex);

        printf("%s Put data in receive queue.\n", output_tag);
    }
}

void * send_work_function (void * input_data)
{
    // Unpack input_data.
    struct socket_data * data = (struct socket_data * )input_data;

    const char * output_tag = "[SEND]:";

    // lock data mutex.
    pthread_mutex_lock(data->data_mutex);

    for(;;) { // waiting for data loop.

        if (send_list == NULL) { // Sleep, nothing to send.
            printf("%s Wait for data.\n", output_tag);
            pthread_cond_wait(data->data_to_send_sig, data->data_mutex);
        }

        // Has lock here if awoken.
        printf("%s There is data!\n", output_tag);

        // Use socket descriptor from receive thread.
        int new_socket_descriptor = *data->new_socket_descriptor;

        struct data_node * current = send_list;

        uint32_t data_size = current->size;
        uint32_t send_data[data_size+1]; // Leave room for length in front.

        uint32_t * data_to_be_sent = current->data;
        send_data[0] = htonl(data_size);
        // Add rest of data to send_data.
        for (uint32_t i = 0; i < data_size; i++) {
            send_data[i+1] = htonl(data_to_be_sent[i]);
            printf("%s Packing data: %" PRIu32 " as :%" PRIu32 ".\n",
                   output_tag,
                   data_to_be_sent[i],
                   send_data[i+1]);
        }
        // Send data.
        printf("%s Sending data of data_size: %" PRIu32 ".\n",
               output_tag,
               data_size+1);
        int status = send(new_socket_descriptor, send_data, (data_size+1)*sizeof(uint32_t), 0);
        if (status == -1) {
            perror("send");
        }

        // Advance the list pointer.
        send_list = current->next;

        if (send_list == NULL) { // Make sure that last does not point to free'd data.
            send_last = NULL;
        }

        // Free the current element.
        free(current->data);
        free(current);

        // Loop until the list is done.
        printf("%s data sent.\n", output_tag);
    }
}

int main(void)
{
    // Create socket;
    int socket_descriptor = 0;

    struct socket_data data = {0};
    struct sockaddr_storage their_address;

    const char * output_tag = "[MAIN]:";

    // Create data lock.
    pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Create signals.
    pthread_cond_t data_to_send_sig = PTHREAD_COND_INITIALIZER;
    pthread_cond_t data_was_received_sig = PTHREAD_COND_INITIALIZER;

    // Populate the socket_data struct with concurrency objects.
    data.data_to_send_sig = &data_to_send_sig;
    data.data_was_received_sig = &data_was_received_sig;
    data.data_mutex = &data_mutex;

    // Populate the socket_data with data related to socket communication.
    data.their_address = &their_address;
    data.socket_descriptor = &socket_descriptor;
    open_socket(OUR_PORT, &data);

    // Create and run receive thread, populates data necessary for send thread.
    printf("%s Created thread for receiving data.\n", output_tag);
    pthread_t receive = {0};
    pthread_create(&receive, NULL, receive_work_function, &data);

    // Create and run send thread.
    printf("%s Created thread for sending data.\n", output_tag);
    pthread_t send = {0};
    pthread_create(&send, NULL, send_work_function, &data);

    // Take data mutex.
    pthread_mutex_lock(&data_mutex);

    for(;;) { // Main loop data-processing loop.
        if (receive_list == NULL) { // Nothing to process.
            pthread_cond_wait(&data_was_received_sig, &data_mutex);
        }

        // Move received data to send queue for the time being.
        if (send_list == NULL) {
            send_list = receive_list;
            send_last = send_list;
        } else { // Already populated.
            // Append first of receive_list to send_list.
            send_last->next = receive_list;
            // Move the send_last pointer.
            send_last = send_last->next;
            // Unlink the moved nodes next pointer.
            send_last->next = NULL;
        }
        // Advance the receive_list pointer.
        receive_list = receive_list->next;

        // Signal the send thread.
        pthread_cond_signal(&data_to_send_sig);

        printf("%s Moved data from receive to send queue.\n", output_tag);
    }

    // Join threads.
    pthread_join(receive, NULL);
    pthread_join(send, NULL);
}
