/******************************************************************************
* echo_client.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo client.  The  *
*              client connects to an arbitrary <host,port> and sends input    *
*              from stdin.                                                    *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define ECHO_PORT 9999
#define BUF_SIZE 4096

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <server-ip> <port> <file-path>\n",argv[0]);
        return EXIT_FAILURE;
    }

    char buf[BUF_SIZE];
        
    int status, sock;
    struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *servinfo; //will point to the results
    hints.ai_family = AF_INET;  //IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP stream sockets
    hints.ai_flags = AI_PASSIVE; //fill in my IP for me

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    if((sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        fprintf(stderr, "Socket failed\n");
        return EXIT_FAILURE;
    }
    
    if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        fprintf(stderr, "Connect\n");
        return EXIT_FAILURE;
    }

    // 评测平台是通过文件去检测的
    int fd = open(argv[3], O_RDONLY); 
    char msg[BUF_SIZE], send_buf[BUF_SIZE];
    // 读文件
    read(fd, msg, BUF_SIZE);
    strcpy(send_buf, msg);

    send(sock, send_buf, strlen(send_buf), 0);
    fprintf(stdout, "Sending:\n%s\n", send_buf);

    // 循环接收响应
    while(1){
        int bytes_received = recv(sock, buf, BUF_SIZE, 0);
        if (bytes_received <= 0) {
            break;  // 连接关闭或出错
        }
        buf[bytes_received] = '\0';
        fprintf(stdout, "Received:\n%s\n", buf);
        memset(buf, 0, BUF_SIZE);
    }

    freeaddrinfo(servinfo);
    close(sock);    
    return EXIT_SUCCESS;
}
