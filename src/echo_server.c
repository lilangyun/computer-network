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
#include <arpa/inet.h>
#include <time.h>
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

// 辅助函数
void parse_fileType(char *fileName, char *fileType);
int add_header(Request *request, char *sbuf, char *file_path);
int close_socket(int sock);

// 模块函数
int handle_request(Request *request, char *buf, int request_size, char *sbuf, int client_sock, const char *client_ip);

// 主函数
int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char recv_buf[BUF_SIZE];   // 接收Buf
    char send_buf[BUF_SIZE];  // 发送Buf

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

    /* finally, loop waiting for input and then write it back */
    while(1)
    {
        cli_size = sizeof(cli_addr);
        if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                    &cli_size)) == -1)
        {
            close(sock);
            close_log();
            fprintf(stderr, "Error accepting connection.\n");
            return EXIT_FAILURE;
        }

        // 生成 IP 字符串
        char client_ip[16];
        strcpy(client_ip, inet_ntoa(cli_addr.sin_addr));
        fprintf(stderr, "%s\n", client_ip);

        // 初始化
        readret = 0;         // 返回值
        int keep_alive = 1;  // 是否持久连接
        int parsed = 0;      // 已经解析过的字符数量
        int total_len = 0;

        // Server
        while(keep_alive){
            // 接收模块
            readret = recv(client_sock, recv_buf + total_len, BUF_SIZE - total_len, 0);
            // 是否终止
            if(readret <= 0){
                break;
            }

            total_len += readret;
            // 流水线处理
            while(parsed < total_len){
                // 未解析完
                int previous_parsed = parsed;
                // 继续解析
                Request *request = parse_pipeline(recv_buf, total_len, &parsed, client_sock);

                // 处理模块
                int request_size = parsed - previous_parsed;
                keep_alive = handle_request(request, recv_buf + previous_parsed, request_size, send_buf, client_sock, client_ip);

                // 如果当前请求为错误请求 -- 识别下一条请求
                if(request == NULL){
                    while(parsed + 4 <= total_len){
                        if(memcmp(recv_buf + parsed, "\r\n\r\n", 4) == 0){
                            parsed += 4;
                            break;
                        }
                        parsed++;
                    }
                }
                
                // 当前请求为正确格式 -- 释放request
                if(request != NULL){
                    free(request->headers);
                    free(request);
                }
                memset(send_buf, 0, BUF_SIZE);
            }
            // 解析完毕 -- 重置
            if(parsed >= total_len){
                parsed = 0;
                total_len = 0;
                memset(recv_buf, 0, BUF_SIZE);
            }
        }

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            close_log();
            fprintf(stderr, "Error reading from client socket.\n");
            return EXIT_FAILURE;
        }

        if (close_socket(client_sock))
        {
            close_socket(sock);
            close_log();
            fprintf(stderr, "Error closing client socket.\n");
            return EXIT_FAILURE;
        }
    }

    close_socket(sock);  // 关闭监听套接字
    close_log();  // 关闭日志模块

    return EXIT_SUCCESS;
}

// 响应模块
int handle_request(Request *request, char *buf, int request_size, char *sbuf, int client_sock, const char *client_ip){
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
    if(strcmp(request->http_method, "GET") == 0 || strcmp(request->http_method, "HEAD") == 0){
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