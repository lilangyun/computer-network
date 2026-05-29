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
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include "parse.h"
#include "log.h"

#define ECHO_PORT 9999
#define BUF_SIZE 10000

const char *RESPONSE_200 = "HTTP/1.1 200 OK\r\n";
const char *RESPONSE_400 = "HTTP/1.1 400 Bad request\r\n\r\n";
const char *RESPONSE_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
const char *RESPONSE_501 = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char *RESPONSE_505 = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

const char *Header_Default = "Content-Length";

typedef struct client_context{
    int socket;
    char ip[16];
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];
    int keep_alive;
    int parsed;
    int total_len;
}client_context;

int server_socket;  // 当前服务器对应的socket

// 辅助函数
void parse_fileType(char *fileName, char *fileType);
int add_header(Request *request, char *sbuf, char *file_path);
int close_socket(int sock);
int find_client(int socket, client_context *client_set);

// 模块函数
int handle_request(Request *request, char *buf, int request_size, char *sbuf, int client_sock, const char *client_ip);

// 信号处理函数
void sig_handler(int sig_num);

// 主函数
int main(int argc, char* argv[])
{
    /* Signal register */
    signal(SIGINT, sig_handler);

    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;

    fprintf(stdout, "----- Liso Server -----\n");
    // 开启日志模块
    init_log();
    
    /* all networked programs must create a socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        close_log();
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    // 设置全局sock -- 便于信号处理
    server_socket = sock;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        close_log();
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        close_log();
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    client_context *client_set = malloc(sizeof(client_context) * FD_SETSIZE);
    for(int i = 0; i < FD_SETSIZE; i++){
        client_set[i].socket = -1;
    }
    fd_set read_set, ready_set;
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);
    int fd_max = sock;
    int ready_num = 0;

    while(1){
        ready_set = read_set;
        ready_num = select(fd_max + 1, &ready_set, NULL, NULL, NULL);
        if(ready_num > 0 && FD_ISSET(sock, &ready_set)){
            cli_size = sizeof(cli_addr);
            if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                    &cli_size)) == -1)
            {
                close(sock);
                close_log();
                fprintf(stderr, "Error accepting connection.\n");
                return EXIT_FAILURE;
            }
            // 设置 socket 超时
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 10000;  // 1*10^4 微秒 = 0.01 秒
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            for(int i = 0; i < FD_SETSIZE; i++){
                if(client_set[i].socket == -1){
                    client_set[i].socket = client_sock;
                    strcpy(client_set[i].ip, inet_ntoa(cli_addr.sin_addr));
                    memset(client_set[i].recv_buf, 0, BUF_SIZE);
                    memset(client_set[i].send_buf, 0, BUF_SIZE);
                    client_set[i].keep_alive = 1;
                    client_set[i].parsed = 0;
                    client_set[i].total_len = 0;
                    break;
                }
            }
            FD_SET(client_sock, &read_set);
            fd_max = client_sock > fd_max ? client_sock : fd_max; 
        }
        else if(ready_num > 0){
            for(int i = sock + 1; i <= fd_max; i++){
                if(FD_ISSET(i, &ready_set)){
                    int t = find_client(i, client_set);
                    if(t == -1) continue;
                    readret = 0;
                    while(client_set[t].keep_alive){
                        readret = recv(i, 
                                       client_set[t].recv_buf + client_set[t].total_len, 
                                       BUF_SIZE - client_set[t].total_len, 
                                       0);
                        if(readret <= 0){
                            break;
                        }
                        client_set[t].total_len += readret;
                        while(client_set[t].parsed < client_set[t].total_len){
                            int previous_parsed = client_set[t].parsed;
                            Request *request = parse_pipeline(client_set[t].recv_buf, 
                                                              client_set[t].total_len, 
                                                              &client_set[t].parsed, 
                                                              i);
                            int request_size = client_set[t].parsed - previous_parsed;
                            client_set[t].keep_alive = handle_request(request, 
                                                                      client_set[t].recv_buf + previous_parsed, 
                                                                      request_size, 
                                                                      client_set[t].send_buf, 
                                                                      i, 
                                                                      client_set[t].ip);
                            if(request == NULL){
                                while(client_set[t].parsed + 4 <= client_set[t].total_len){
                                    if(!memcmp(client_set[t].recv_buf + client_set[t].parsed, 
                                               "\r\n\r\n", 
                                               4)){
                                        client_set[t].parsed += 4;
                                        break;
                                    }
                                    client_set[t].parsed++;
                                }
                            }
                            if(request != NULL){
                                free(request->headers);
                                free(request);
                            }
                            memset(client_set[t].send_buf, 0, BUF_SIZE);
                        }
                        if(client_set[t].parsed >= client_set[t].total_len){
                            client_set[t].parsed = 0;
                            client_set[t].total_len = 0;
                            memset(client_set[t].recv_buf, 0, BUF_SIZE);
                        }
                    }
                    // Parsing finish
                    FD_CLR(i, &read_set);
                    client_set[t].socket = -1;
                    client_set[t].keep_alive = 0;
                    close_socket(i);
                    if(readret == 0){
                        fprintf(stderr, "Recv ends normally.\n");
                    }
                    else if(readret == -1){
                        fprintf(stderr, "Recv error.\n");
                    }
                    else{
                        fprintf(stderr, "Keep-alive close.\n");
                    }
                    continue;
                }
            }
        }
    }
    for(int i = sock; i <= fd_max; i++){
        if(close_socket(i)){
            fprintf(stderr, "Close fd %d error.\n", i);
        }
    }
    close_log();  // 关闭日志模块
    return EXIT_SUCCESS;
}

// 响应模块
int handle_request(Request *request, 
                   char *buf, 
                   int request_size, 
                   char *sbuf, 
                   int client_sock, 
                   const char *client_ip){
    // 错误处理
    if(request == NULL){
        strcpy(sbuf, RESPONSE_400);
        send(client_sock, sbuf, strlen(sbuf), 0);
        return 1;
    }

    // 组装请求行 -- 传入日志模块
    char request_line[256];
    sprintf(request_line, "%s %s %s",request->http_method, request->http_uri, request->http_version);

    // 分支 -- 判断是哪种请求
    if(strcmp(request->http_method, "GET") == 0 || 
       strcmp(request->http_method, "HEAD") == 0){
        if(strcmp(request->http_version, "HTTP/1.1") != 0){
            strcpy(sbuf, RESPONSE_505);
            send(client_sock, sbuf, strlen(sbuf), 0);
            access_log(client_ip, request_line, 505, 0);
            return 1;
        }

        char file_path[1024] = "./static_site";
        char *uri = request->http_uri;
        if(uri[0] == '/') uri++;
        if(strlen(uri) == 0) {
            strcat(file_path, "/index.html");
        } 
        else {
            strcat(file_path, "/");
            strcat(file_path, uri);
        }

        fprintf(stderr, "File_path: %s.\n", file_path);
        int fd = open(file_path, O_RDONLY);
        fprintf(stderr, "fd = %d\n", fd);
        // 文件无法打开
        if(fd < 0){
            strcpy(sbuf, RESPONSE_404);
            send(client_sock, sbuf, strlen(sbuf), 0);
            error_log(client_ip, "File Not Found", file_path);
            return 1;
        }
        char data[BUF_SIZE];
        int readret = read(fd, data, BUF_SIZE);
        if(readret < 0){
            error_log(client_ip, "File can't read", file_path);
            close(fd);
            return 1;
        }
        data[readret] = '\0';
        // 添加头部
        strcpy(sbuf, RESPONSE_200);
        int ret = 0;
        if(add_header(request, sbuf, file_path)) {
            // 需要持久连接
            ret = 1;
        }
        if(strcmp(request->http_method, "GET") == 0){
            // GET 数据
            strcat(sbuf, data);
        }
        send(client_sock, sbuf, strlen(sbuf), 0);

        // access 日志记录
        struct stat st;
        stat(file_path, &st);
        access_log(client_ip, request_line, 200, st.st_size);
        return ret;
    }
    else if(strcmp(request->http_method, "POST") == 0){
        if(strcmp(request->http_version, "HTTP/1.1") != 0){
            strcpy(sbuf, RESPONSE_505);
            send(client_sock, sbuf, strlen(sbuf), 0);
            access_log(client_ip, request_line, 505, 0);
            return 1;
        }
        // echo
        strncpy(sbuf, buf, request_size);
        send(client_sock, sbuf, strlen(sbuf), 0);
        access_log(client_ip, request_line, 200, strlen(sbuf));
    }
    else{
        // 未实现的请求方法
        strcpy(sbuf, RESPONSE_501);
        send(client_sock, sbuf, strlen(sbuf), 0);
        access_log(client_ip, request_line, 501, 0);
        return 1;
    }
    return 1;
}

// 判断文件类型
void parse_fileType(char *fileName, char *fileType)
{
    if(strstr(fileName, ".html")) {
        strcpy(fileType, "text/html");
    }
    else if(strstr(fileName, ".css")) {
        strcpy(fileType, "text/css");
    }
    else if(strstr(fileName, ".png")) {
        strcpy(fileType, "image/png");
    }
    else if(strstr(fileName, ".jpg") || strstr(fileName, ".jpeg")) {
        strcpy(fileType, "image/jpeg");
    }
    else if(strstr(fileName, ".gif")) {
        strcpy(fileType, "image/gif");
    }
    else {
        strcpy(fileType, "text/plain");
    }
}

// 添加头部组件
int add_header(Request *request, char *sbuf, char *file_path){
    int i = 0, keep_alive = 1;  // ret 用来记录是否保持连接
    char date[64]; char fileType[64]; char temp[64];
    char connect_msg[64] = "Connection: keep-alive\r\n";
    time_t now_time = time(NULL);
    for( ; (request != NULL) && (i < request->header_count); i++){
        if(strcmp(request->headers[i].header_name, "Connection") == 0){
            if(strcmp(request->headers[i].header_value, "close") == 0) {
                keep_alive = 0;
                strcpy(connect_msg, "Connection: close\r\n");
            }
            break;
        }
    }

    // Server
    strcat(sbuf, "Server: Liso/1.1\r\n");

    // Date
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", gmtime(&now_time));
    sprintf(temp, "Date: %s\r\n", date);
    strcat(sbuf, temp);
    memset(temp, 0, 64);

    // Connection
    strcat(sbuf, connect_msg);

    // Content-Type
    if(file_path != NULL) {
        parse_fileType(file_path, fileType);
        sprintf(temp, "Content-Type: %s\r\n", fileType);
        strcat(sbuf, temp);
        memset(temp, 0, 64);
    }

    // Content-Length
    struct stat st;
    if(file_path != NULL && stat(file_path, &st) == 0){
        sprintf(temp, "Content-Length: %ld\r\n", st.st_size);
        strcat(sbuf, temp);
        memset(temp, 0, 64);
    }
    strcat(sbuf, "\r\n");

    return keep_alive;
}

// 关闭socket
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

// 在client_set中找到对应于socket的client
int find_client(int socket, client_context *client_set){
    for(int t = 0; t < FD_SETSIZE; t++){
        if(client_set[t].socket == socket){
            return t;
        }
    }
    return -1;
}

// 信号处理函数
void sig_handler(int sig_num){
    close_socket(server_socket);
    close_log();
    fprintf(stderr, "SIGINT hits me.\n", sig_num);
    exit(0);
}