/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "parse.h"

#define ECHO_PORT 9999
#define BUF_SIZE 4096

const char *RESPONSE_400 = "HTTP/1.1 400 Bad request\r\n\r\n";
const char *RESPONSE_501 = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char *RESPONSE_200 = "HTTP/1.1 200 OK\r\n";
const char *RESPONSE_505 = "HTTP/1.1 505 HTTP Version not supported\r\n";
const char *RESPONSE_404 = "HTTP/1.1 404 Not Found\r\n\r\n";

const char *URI_Default = "static_site/index.html";

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    fprintf(stdout, "----- Echo Server -----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        cli_size = sizeof(cli_addr);
        if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                    &cli_size)) == -1)
        {
            close(sock);
            fprintf(stderr, "Error accepting connection.\n");
            return EXIT_FAILURE;
        }

        readret = 0;

        // recv_begin
        if((readret = recv(client_sock, buf, BUF_SIZE, 0)) >= 1)
        {
            // parse
            Request *request = parse(buf, readret, client_sock);
            int len;

            // response
            if(request == NULL){  //  Invalid format
                len = send(client_sock, RESPONSE_400, strlen(RESPONSE_400), 0);
                if(len < 0) break;
                fprintf(stdout, "Sending %s", RESPONSE_400);  // 400
            }
            else if( strcmp(request->http_method, "GET")==0 ){
                len = GET(request, client_sock);
                if(len < 0) break;
            }
            else if(strcmp(request->http_method, "HEAD")==0){
                // TODO
                len = GET(request, client_sock);
                if(len < 0) break;
            }
            else if(strcmp(request->http_method, "POST")==0){
                // echo
                len = send(client_sock, buf, readret, 0);
                if(len < 0) break;
                fprintf(stdout, "Sending %s", buf);
            }
            else{  //  Not performed
                len = send(client_sock, RESPONSE_501, strlen(RESPONSE_501), 0);
                if(len < 0) break;
                fprintf(stdout, "Sending %s", RESPONSE_501);  // 501
            }
            // response_end
            
            memset(buf, 0, BUF_SIZE);
        }
        // recv_end

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Error reading from client socket.\n");
            return EXIT_FAILURE;
        }

        if (close_socket(client_sock))
        {
            close_socket(sock);
            fprintf(stderr, "Error closing client socket.\n");
            return EXIT_FAILURE;
        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}

// GET Method
int GET(Request *request, int client_sock){
    int len;
    // Check Version
    if(strcmp(request->http_version, "HTTP/1.1") != 0){
        len = send(client_sock, RESPONSE_505, strlen(RESPONSE_505), 0);
        if(len < 0){
            return len;
        }
        fprintf(stdout, "Sending %s",RESPONSE_505);
    }
    // end

    // Reap data
    int fd = open(URI_Default, O_RDONLY);
    char buf[8192];
    int readret = read(fd, buf, 8192);
    char *response = (char *)malloc( (8192+strlen(RESPONSE_200)) * sizeof(char));
    strcpy(response, RESPONSE_200);
    strcat(response, buf);
    len = send(client_sock, response, strlen(response), 0);
    if(len < 0){
        return len;
    }
    fprintf(stdout, "Sending %s", buf);
    // end
    
    return len;
}

int HEAD(Request *request, int client_sock){
     int len;
    // Check Version
    if(strcmp(request->http_version, "HTTP/1.1") != 0){
        len = send(client_sock, RESPONSE_505, strlen(RESPONSE_505), 0);
        if(len < 0){
            return len;
        }
        fprintf(stdout, "Sending %s",RESPONSE_505);
    }
    // end

    // Reap data
    struct stat* Stat;
    char buf[8192];
    int statret = stat(URI_Default, Stat);
    // Stat->;
    char *response = (char *)malloc( (8192+strlen(RESPONSE_200)) * sizeof(char));
    strcpy(response, RESPONSE_200);
    strcat(response, buf);
    len = send(client_sock, response, strlen(response), 0);
    if(len < 0){
        return len;
    }
    fprintf(stdout, "Sending %s", buf);
    // end
    
    return len;
}
