#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "safequeue.h"
#include "proxyserver.h"


/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */

struct ListenerThreadInfo {
    int server_fd;
    int port;
};

int num_listener;
int *listener_ports;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;
struct ListenerThreadInfo* ListenerThreadInfoList; 

void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    return;
}

void* serve_request(void *arg) {
    while(1) { 
    struct request r = get_work();
    // create a fileserver socket
    if(r.delay > 0){
        sleep(r.delay);
    }
    int client_fd = r.fd;
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
                                
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
        return NULL;
    }

    // successfully connected to the file server
    char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));

    // forward the client request to the fileserver
    // int bytes_read = read(r.fd, buffer, RESPONSE_BUFSIZE);
    int ret = http_send_data(fileserver_fd, r.data, strlen(r.data));
    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");

    } else {
        // forward the fileserver response to the client
        while (1) {
            int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            ret = http_send_data(client_fd, buffer, bytes_read);
            if (ret < 0) { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
    // Free resources and exit
    free(buffer);
    // sleep(10);
    }
    return NULL;
}

char *getPath(char *data) {
    char *token = strtok(data, "\r\n");
    char *path = NULL;
    if (token != NULL) {
        int size = strlen(token);
        char *s1 = strstr(token, " ");  //request method
        char *s2 = strstr(s1 + 1, " "); //path
        size = s2 - s1 - 1;
        path = strndup(s1 + 1, size);
    }
    return path;
}

int server_fd;
/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void *serve_forever(void *listener_thread_info_ptr)
{
    struct ListenerThreadInfo *listener_thread_info = (struct ListenerThreadInfo *)listener_thread_info_ptr;
    listener_thread_info->server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listener_thread_info->server_fd == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(listener_thread_info->server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }


    int proxy_port = listener_thread_info->port;
    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    proxy_address.sin_port = htons(proxy_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(listener_thread_info->server_fd, (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(listener_thread_info->server_fd, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1) {
        client_fd = accept(listener_thread_info->server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        printf("accepted conn %d client \n", client_fd);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);
        
        int delay, priority;
        char *data = parse_client_request(client_fd, &delay, &priority);
        if(priority == -1) {
            char *r = get_max();
            if(r == NULL) {
                http_start_response(client_fd, QUEUE_EMPTY);
            } else {
                http_start_response(client_fd, BAD_REQUEST);
                http_send_header(client_fd, "Content-type", "text/html");
                 http_send_header(client_fd, "Server", "httpserver/1.0");
                http_end_headers(client_fd);
                // printf("sending data \n");
                // char *test = "jhgsdfjhgsdjhfgjhsdf ";
                http_send_string(client_fd, getPath(r));
            }
            close(client_fd);
        } else {
        int ret = add_work(client_fd, priority, delay, data);
        if(ret == 1) {
            //queue is full
            http_start_response(client_fd, QUEUE_FULL);
            close(client_fd);
        }
        }
        
    }

    shutdown(listener_thread_info->server_fd, SHUT_RDWR);
    close(listener_thread_info->server_fd);
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            free(ListenerThreadInfoList);
            ListenerThreadInfoList = (struct ListenerThreadInfo *)malloc(num_listener * sizeof(struct ListenerThreadInfo));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
                ListenerThreadInfoList[j].server_fd = -1;
                ListenerThreadInfoList[j].port = listener_ports[j];
            }
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }
    print_settings();
    create_queue();
    printf("creating producer threads\n");
    // serve_forever(&server_fd);
    for (int i = 0; i < num_listener; ++i) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, serve_forever, &ListenerThreadInfoList[i]) != 0) {
            perror("Error creating thread");
            return EXIT_FAILURE;
        }

        pthread_detach(thread);
    }

    printf("creating consumer threads\n");
    for (int i = 0; i < num_workers; ++i) 
    {
        pthread_t thread;
        if (pthread_create(&thread, NULL, serve_request, NULL) != 0) {
            perror("Error creating thread");
            return EXIT_FAILURE;
        }

        pthread_detach(thread);
    }

    while (1) {
        sleep(1); 
    }
    return EXIT_SUCCESS;
}
