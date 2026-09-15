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
#include "response.h"

extern int server_socket;

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

    // 设置端口复用
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Failed to set SO_REUSEADDR\n");
        close_socket(sock);
        close_log();
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
            // 绑定client_sock
            bind_client_context(client_set, client_sock, cli_addr);
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
                            // 有可能超时
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
                            // 是否保持持久连接
                            client_set[t].keep_alive = handle_request(request, 
                                                                      client_set[t].recv_buf + previous_parsed, 
                                                                      request_size, 
                                                                      client_set[t].send_buf, 
                                                                      i,  // 文件描述符
                                                                      client_set[t].ip);
                            // 过滤错误请求 -- 移位至下一条请求
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
                        // 释放接收缓冲区
                        if(client_set[t].parsed >= client_set[t].total_len){
                            client_set[t].parsed = 0;
                            client_set[t].total_len = 0;
                            memset(client_set[t].recv_buf, 0, BUF_SIZE);
                        }
                    }
                    // 释放槽位
                    FD_CLR(i, &read_set);
                    client_set[t].socket = -1;  // 置为无效位
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

