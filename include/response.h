#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
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

// 客户端上下文: socket--ip--缓冲区--解析游标
typedef struct client_context{
    int socket;
    char ip[16];
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];
    int keep_alive;
    int parsed;
    int total_len;
}client_context;

// 解析文件MIME类型 
void parse_fileType(char *fileName, char *fileType);
// 为响应添加头部
int add_header(Request *request, char *sbuf, char *file_path);
// 关于sock
int close_socket(int sock);
// 找到与socket绑定的client
int find_client(int socket, client_context *client_set);
// 模块函数
int handle_request(Request *request, char *buf, int request_size, char *sbuf, int client_sock, const char *client_ip);
// 信号处理函数
void sig_handler(int sig_num);
// 找到空闲客户端context
int bind_client_context(client_context client_set[], int client_sock, struct sockaddr_in cli_addr);
// 向客户端发送500
void send_error_500(int client_socket);
// 执行cgi
void exec_cgi(int client_socket, char *file_name, Request *req, const char *client_ip);
