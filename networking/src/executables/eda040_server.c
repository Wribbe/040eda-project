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
#include <time.h>
#include <math.h>
#include "capture.h"
#include "net_data.h"

#define TEN_TO_NINE 1000000000
#define TEN_TO_SIX 1000000
#define TIMESTAMP_POS 25
#define TIME_ARRAY_SIZE 8

#define SIZE(x) sizeof(x)/sizeof(x[0])

struct timespec PICTURE_INTERVAL = {0};

void * get_in_address(struct sockaddr * sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in * )sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 * )sa)->sin6_addr);
}

void * my_malloc(size_t size) {
    void * data = malloc(size);
    if (data == NULL) {
        fprintf(stderr, "Could not allocate data, aborting.\n");
        exit(1);
    }
    return data;
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

enum PACKET_TYPE {
    COMMAND,
    PICTURE,
};

struct data_node
{
    enum PACKET_TYPE type;
    uint32_t size;
    uint32_t * data;
    struct data_node * next;
};

void set_capture_interval(uint8_t pictures_per_second) {
    printf("[INFO]: Setting capture interval to %" PRIu8 "/s.\n", pictures_per_second);
    // Divide with 1 to get the full number of seconds.
    float seconds_per_picture = 1.0/pictures_per_second;
    PICTURE_INTERVAL.tv_sec = seconds_per_picture/1;
    // Take the remainder and multiply by 10^9 to convert it to nanoseconds.
    PICTURE_INTERVAL.tv_nsec = fmodf(seconds_per_picture, 1.0)*1e9;
}

struct data_node * receive_list = NULL;
struct data_node * receive_last = NULL;
struct data_node * send_list = NULL;
struct data_node * send_last = NULL;
struct data_node * picture_node = NULL;

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
        printf("%s Waiting for data on port: %s.\n", output_tag, OUR_PORT);

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

        uint32_t data_buffer[MAX_DATA_SIZE];

        numbytes = recv(new_socket_descriptor,
                        data_buffer,
                        MAX_DATA_SIZE,
                        0);

        printf("%s Got data from %s\n", output_tag, client_address_buffer);

        if (numbytes == -1) {
            printf("%s Error on recv, client closed connection?\n", output_tag);
            thread_data->new_socket_descriptor = NULL;
            continue;
        }

        if (numbytes == 0) {
            printf("\n###### CONTINUING!!\n\n");
            thread_data->new_socket_descriptor = NULL;
            continue;
        }

        uint32_t * data_pointer = (uint32_t * )data_buffer;
        uint32_t data_size = ntohl(*data_pointer);

        uint32_t * converted_data = my_malloc(sizeof(uint32_t)*data_size);
        // Convert received data.
        for (uint32_t i = 0; i < data_size; i++) {
            converted_data[i] = ntohl(data_pointer[1+i]);
        }

        // Create new node for the receive list.
        struct data_node * node = my_malloc(sizeof(struct data_node));

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

        // Sleep if there is nothing to send or there is no connected client.
        while (send_list == NULL || data->new_socket_descriptor == NULL) {
            if (send_list == NULL) {
                printf("%s Wait for data.\n", output_tag);
            } else {
                printf("%s Have data, but no connected client.\n", output_tag);
            }
            pthread_cond_wait(data->data_to_send_sig, data->data_mutex);
        }

        // Has lock here if awoken.
        printf("%s There is data and a connected client.\n", output_tag);

        // Use socket descriptor from receive thread.
        int new_socket_descriptor = *data->new_socket_descriptor;

        struct data_node * current = send_list;

        uint32_t data_size = current->size;

        // Send data.
        printf("%s Sending data of data_size: %zu.\n",
               output_tag,
               (size_t)data_size);
        int status = send(new_socket_descriptor, current->data, data_size, 0);
        if (status == -1) {
            printf("%s User closed connection.\n", output_tag);
            // Set new_socket_descriptor to NULL.
            data->new_socket_descriptor = NULL;
            continue;
        }

        printf("%s Data sent.\n", output_tag);

        // Advance the list pointer.
        send_list = current->next;

        if (send_list == NULL) { // Make sure that last does not point to free'd data.
            send_last = NULL;
        }

        // Free the processed element.
        free(current->data);
        free(current);

        // Set picture_node to null if it was current.
        if (current == picture_node) {
            picture_node = NULL;
        }
        current = NULL;
    }
}

void * capture_work_function (void * input_data)
{
    // Unpack input data.
    struct socket_data * data = (struct socket_data * )input_data;
    const char * options = "fps=25&sdk=format=Y800&resolution=320X240";

    // Output prefix tag.
    const char * output_tag = "[Capture]:";

    // Time related variables.
    uint64_t time_stamp = 0;
    uint64_t ref_nano = 0;
    size_t timestamp_size = sizeof(uint64_t);

    // Get current system time.
    struct timespec real_time = {0};
    clock_gettime(CLOCK_REALTIME, &real_time);

    // Image variables.
    size_t image_size = 0;
    void * capture_data = NULL;
    // Set up image stream on camera.
    printf("%s Opening up media stream.\n", output_tag);
    media_stream * stream = capture_open_stream(IMAGE_JPEG, options);
    media_frame * image_frame;
    printf("%s Stream open.\n", output_tag);

    // Set current time stamp.
    uint64_t ref_time_millis = 0;
    uint64_t time_stamp_millis = 0;

    // Set reference time.
    ref_time_millis += ((uint64_t)real_time.tv_sec) * 1000;
    ref_time_millis += real_time.tv_nsec / 1e6;

    for (;;) { // Action loop.

        // ### Capturing pictures.

        // Always create local storage for data.
        struct data_node * local_picture = my_malloc(sizeof(struct data_node));

        // Get frame, size and time stamp.
        image_frame = capture_get_frame(stream);
        image_size = capture_frame_size(image_frame);
        time_stamp = (uint64_t)capture_frame_timestamp(image_frame);

        // Total size of image data and time stamp.
        size_t header_flag = 1;
        size_t size_space = 4;
        size_t total_data_size = header_flag+timestamp_size+size_space+image_size;

        // Allocate memory on the heap for image data.
        capture_data = my_malloc(total_data_size);

        // Set up byte_pointer.
        unsigned char * byte_pointer = (unsigned char * )capture_data;

        // Set header flag.
        *byte_pointer = 1;
        byte_pointer += header_flag;

        if (ref_nano == 0) {
            ref_nano = (uint64_t)time_stamp;
        }

        time_stamp_millis = ref_time_millis + ((time_stamp - ref_nano) / 1e6);

        // Copy time stamp data to output data.
        uint32_t converted_time_millis[2];
        uint32_t * u32_pointer = (uint32_t * )&time_stamp_millis;
        uint32_t time_top = *(u32_pointer+1);
        uint32_t time_bottom = *u32_pointer;
        converted_time_millis[0] = htonl(time_top);
        converted_time_millis[1] = htonl(time_bottom);
        memcpy(byte_pointer, &converted_time_millis, sizeof(uint64_t));
        byte_pointer += timestamp_size;

        // Copy image size to data.
        size_t converted_image_size = htonl(image_size);
        memcpy(byte_pointer, &converted_image_size, size_space);
        byte_pointer += size_space;

        // Copy image data from frame to heap.
        memcpy(byte_pointer, capture_frame_data(image_frame), image_size);

        // Free frame resources.
        capture_frame_free(image_frame);

        // Populate local picture node.
        local_picture->size = total_data_size;
        local_picture->data = capture_data;
        local_picture->next = NULL;
        local_picture->type = PICTURE;

        // ### Manipulate send list.

        // Get and lock data mutex.
        pthread_mutex_lock(data->data_mutex);

        // Is there a existing picture node?
        if (picture_node != NULL) { // Use that struct.
            // Free old picture data.
            free(picture_node->data);
            // Replace current data and size.
            picture_node->data = local_picture->data;
            picture_node->size = local_picture->size;
            // Get rid of unused local picture struct.
            free(local_picture);
        } else { // Use created struct.
            picture_node = local_picture;
            // Prepend node to the list.
            if (send_list == NULL) { // Only element in list.
                send_list = picture_node;
                send_last = send_list;
            } else { // More than one element, prepend.
                struct data_node * old_first = send_list;
                send_list = picture_node;
                send_list->next = old_first;
            }
        }

        printf("Size of picture: %zn\n", image_size);

        printf("\r%s Capturing picture, Top part of timestamp: %" PRIu32 " Lower part of timestamp: %" PRIu32,
               output_tag,
               time_top,
               time_bottom);
        fflush(stdout);

        if (data->new_socket_descriptor != NULL) {
            // Signal send thread.
            pthread_cond_signal(data->data_to_send_sig);
            printf("\n");
        }

        // Release data mutex.
        pthread_mutex_unlock(data->data_mutex);

        // Sleep until next picture should be taken.
        nanosleep(&PICTURE_INTERVAL, NULL);
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

    // Set capture interval to default capture.
    uint8_t pictures_per_second = 25;
    set_capture_interval(pictures_per_second);

    // Create and run capture thread.
    printf("%s Created thread for capturing pictures.\n", output_tag);
    pthread_t capture = {0};
    pthread_create(&capture, NULL, capture_work_function, &data);

    // Take data mutex.
    pthread_mutex_lock(&data_mutex);

    for(;;) { // Main loop data-processing loop.
        while (receive_list == NULL) { // Nothing to process.
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
