#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //POSIX API for system calls
#include <arpa/inet.h> //definitions for internet operations
#include <sys/stat.h> //data returned by the stat() function
#include <dirent.h> //directory operations
#include <time.h> //Time and date functions
#include <pthread.h> //POSIX threads
#include <stdint.h> //For intptr_t
#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT" //Date format for HTTP headers
#define MAX_REQUEST_SIZE 4000 //Maximum size of a request
#define MAX_PATH_SIZE 4096 //Maximum size of a file path

//Function declarations
void parse_args(int argc, char *argv[], int *port, int *pool_size, int *max_queue_size, int *max_requests);
void server_socket(int port, int *server_socket);
int handle_client_connection(void *client_socket); //Updated to return int
void send_response(int client_socket, const char *status, const char *status_msg, const char *content_type, const char *body, off_t content_length, const char *location, time_t last_modified);
void send_file_response(int client_socket, const char *file_path, off_t content_length, time_t last_modified);
void send_directory_listing(int client_socket, const char *dir_path, const char *request_path);
void send_error_response(int client_socket, int status_code, const char *status_msg);
char *get_mime_type(const char *name);
void log_message(const char *message);

int main(int argc, char *argv[]) {
    int port, pool_size, max_queue_size, max_requests;
    parse_args(argc, argv, &port, &pool_size, &max_queue_size, &max_requests); //Parse command line arguments

    int server_sock;
    server_socket(port, &server_sock); //Create server socket
    printf("Server socket created and listening on port %d\n", port);

    //Initialize the thread pool
    threadpool *pool = create_threadpool(pool_size, max_queue_size);

    int request_count = 0;
    while (request_count < max_requests) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len); //Accept client connection
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        //Dispatch the client connection to the thread pool
        dispatch(pool, handle_client_connection, (void *)(intptr_t)client_sock);
        request_count++;
    }

    //Cleanup
    close(server_sock);
    destroy_threadpool(pool);
    return 0;
}

void parse_args(int argc, char *argv[], int *port, int *pool_size, int *max_queue_size, int *max_requests) {
    if (argc != 5) {
        fprintf(stderr, "Usage: server <port> <pool-size> <max-queue-size> <max-number-of-request>\n");
        exit(EXIT_FAILURE);
    }

    *port = atoi(argv[1]); //Convert port argument to integer
    *pool_size = atoi(argv[2]); //Convert pool size argument to integer
    *max_queue_size = atoi(argv[3]); //Convert max queue size argument to integer
    *max_requests = atoi(argv[4]); //Convert max requests argument to integer

    if (*port <= 0 || *port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", *port);
        exit(EXIT_FAILURE);
    }
    if (*pool_size <= 0) {
        fprintf(stderr, "Invalid pool size: %d\n", *pool_size);
        exit(EXIT_FAILURE);
    }
    if (*max_queue_size <= 0) {
        fprintf(stderr, "Invalid max queue size: %d\n", *max_queue_size);
        exit(EXIT_FAILURE);
    }
    if (*max_requests <= 0) {
        fprintf(stderr, "Invalid max number of requests: %d\n", *max_requests);
        exit(EXIT_FAILURE);
    }
}

void server_socket(int port, int *server_socket) {
    *server_socket = socket(AF_INET, SOCK_STREAM, 0); //Create socket
    if (*server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(*server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { //Set socket options
        perror("setsockopt");
        close(*server_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(*server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { //Bind socket to address
        perror("bind");
        close(*server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(*server_socket, 10) < 0) { //listen for connections
        perror("listen");
        close(*server_socket);
        exit(EXIT_FAILURE);
    }
}

int handle_client_connection(void *client_socket) {
    int sock = (intptr_t)client_socket; //This allows the function to use sock as a file descriptor for the client socket.
    char request[MAX_REQUEST_SIZE + 1];//to hold the request data read from the client socket
    int bytes_read = 0;
    char *ptr = request;//This pointer will be used to read data into the request array
    int remaining = MAX_REQUEST_SIZE;

    //read the request line until "\r\n"
    while (remaining > 0) {
        int n = read(sock, ptr, 1);//reads one byte from the client socket into the location pointed to by ptr
        if (n <= 0) {
            send_error_response(sock, 500, "Internal Server Error");
            close(sock);
            return 1;
        }
        ptr++;
        remaining--;
        if (*(ptr - 2) == '\r' && *(ptr - 1) == '\n') { //checks if the last two characters read are \r and \n, which indicate the end of the request line
            break;
        }
    }
    *ptr = '\0';

    //parse the request line
    char method[10], path[MAX_PATH_SIZE], protocol[10];//These will hold the HTTP method, the requested path and the protocol version
    if (sscanf(request, "%9s %4095s %9s", method, path, protocol) != 3) {//to parse the request line into three components: method, path, and protocol
        send_error_response(sock, 400, "Bad Request");
        close(sock);
        return 1;
    }

    //validate protocol
    if (strcmp(protocol, "HTTP/1.0") != 0 && strcmp(protocol, "HTTP/1.1") != 0) {//checks if the protocol is either "HTTP/1.0" or "HTTP/1.1"
      //If it is not, the function sends a "400 Bad Request" response
        send_error_response(sock, 400, "Bad Request");
        close(sock);
        return 1;
    }

    //checks if the method is not "GET"... If the condition is true, the method is not supported
    if (strcmp(method, "GET") != 0) {
        send_error_response(sock, 501, "Not Supported");
        close(sock);
        return 1;
    }


    char full_path[MAX_PATH_SIZE];//to hold the full path of the requested resource
    snprintf(full_path, sizeof(full_path), ".%s", path);//constructs the full path by prepending a dot (.) to the path and stores it in full_path

    //check if the path exists
    struct stat st;//to hold information about the file

    if (stat(full_path, &st) < 0) {//to get information about the file at full_path. If stat returns a negative value, the file does not exist.
        send_error_response(sock, 404, "Not Found");
        close(sock);
        return 1;
    }

    //handle directories
    if (S_ISDIR(st.st_mode)) { //check if the requested path is a directory using the S_ISDIR macro
        if (path[strlen(path) - 1] != '/') { //checks if the path does not end with a slash
            char new_location[MAX_PATH_SIZE];//to hold the new location path
            snprintf(new_location, sizeof(new_location), "%s/", path); //constructs the new location path by appending a slash (/) to the path and stores it in new_location

            send_response(sock, "302", "Found", NULL, NULL, 0, new_location, 0);//sends a "302 Found" response to the client with the new location
        }
        else {
            char index_path[MAX_PATH_SIZE + 10];
            snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
            if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) { //if the index.html file exists and is a regular file using the stat function and the S_ISREG macro
                send_file_response(sock, index_path, st.st_size, st.st_mtime);
            } else {
              //return the contents of the directory in the format as in file dir_content.txt
                send_directory_listing(sock, full_path, path);
            }
        }
    }
    //Handle files
    else if (S_ISREG(st.st_mode)) { //checks if the requested path is a regular file
        if (access(full_path, R_OK) < 0) {//checks if the file is readable using the access function
            send_error_response(sock, 403, "Forbidden");
        } else { //the file is readable
            send_file_response(sock, full_path, st.st_size, st.st_mtime);
        }
    }
    else {//the requested path is neither a directory nor a regular file
        send_error_response(sock, 403, "Forbidden");
    }

    close(sock);
    return 0;
}
/* send_response():
* The function constructs the HTTP response header by appending various headers based on the provided parameters.
* It includes the status line, server information, date, location (if provided), content type (if provided), content length, last modified time (if provided), and connection header.
* It sends the constructed response header to the client.
* If a response body is provided, it sends the body to the client.
* */
void send_response(int client_socket, const char *status, const char *status_msg, const char *content_type, const char *body, off_t content_length, const char *location, time_t last_modified) {
    time_t now;
    char timebuf[128];
    now = time(NULL);
    //it formats the current time (now) into a string (timebuf) using the strftime function and the RFC1123FMT format
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

    char response[8192];
    int response_len = snprintf(response, sizeof(response),
                                "HTTP/1.0 %s %s\r\n"
                                "Server: webserver/1.0\r\n"
                                "Date: %s\r\n",
                                status, status_msg, timebuf);

    if (location) {
        response_len += snprintf(response + response_len, sizeof(response) - response_len,
                                 "Location: %s\r\n", location);
    }

    if (content_type) {
        response_len += snprintf(response + response_len, sizeof(response) - response_len,
                                 "Content-Type: %s\r\n", content_type);
    }

    response_len += snprintf(response + response_len, sizeof(response) - response_len,
                             "Content-Length: %ld\r\n", content_length);

    if (body && last_modified > 0) {
        char last_modified_str[128];
        strftime(last_modified_str, sizeof(last_modified_str), RFC1123FMT, gmtime(&last_modified));
        response_len += snprintf(response + response_len, sizeof(response) - response_len,
                                 "Last-Modified: %s\r\n", last_modified_str);
    }

    response_len += snprintf(response + response_len, sizeof(response) - response_len,
                             "Connection: close\r\n\r\n");

    send(client_socket, response, response_len, 0);

    if (body) {
        send(client_socket, body, content_length, 0);
    }
}

void send_file_response(int client_socket, const char *file_path, off_t content_length, time_t last_modified) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        send_error_response(client_socket, 500, "Internal Server Error");
        return;
    }

    char *mime_type = get_mime_type(file_path);
    send_response(client_socket, "200", "OK", mime_type, NULL, content_length, NULL, last_modified);

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
}

void send_directory_listing(int client_socket, const char *dir_path, const char *request_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        send_error_response(client_socket, 500, "Internal Server Error");
        return;
    }

    char response_body[8192] = "<html><body><h1>Directory Listing</h1><ul>";
    int response_len = strlen(response_body);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        response_len += snprintf(response_body + response_len, sizeof(response_body) - response_len,
                                 "<li><a href=\"%s/%s\">%s</a></li>", request_path, entry->d_name, entry->d_name);
    }
    closedir(dir);

    response_len += snprintf(response_body + response_len, sizeof(response_body) - response_len, "</ul></body></html>");

    send_response(client_socket, "200", "OK", "text/html", response_body, response_len, NULL, 0);
}

void send_error_response(int client_socket, int status_code, const char *status_msg) {
    const char *body;
    switch (status_code) {
        case 400:
            body = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD><BODY><H4>400 Bad Request</H4>Bad request.</BODY></HTML>";
        break;
        case 403:
            body = "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD><BODY><H4>403 Forbidden</H4>Access denied.</BODY></HTML>";
        break;
        case 404:
            body = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD><BODY><H4>404 Not Found</H4>File not found.</BODY></HTML>";
        break;
        case 500:
            body = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD><BODY><H4>500 Internal Server Error</H4>Some server side error.</BODY></HTML>";
        break;
        case 501:
            body = "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD><BODY><H4>501 Not supported</H4>Method is not supported.</BODY></HTML>";
        break;
        default:
            body = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD><BODY><H4>500 Internal Server Error</H4>Some server side error.</BODY></HTML>";
        break;
    }
    send_response(client_socket, status_code == 302 ? "302" : "400", status_msg, "text/html", body, strlen(body), NULL, 0);
}


char *get_mime_type(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void log_message(const char *message) {
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] %s\n", timebuf, message);
}
