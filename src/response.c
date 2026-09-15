#include "response.h"

const char *RESPONSE_200 = "HTTP/1.1 200 OK\r\n";
const char *RESPONSE_400 = "HTTP/1.1 400 Bad request\r\n\r\n";
const char *RESPONSE_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
const char *RESPONSE_501 = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char *RESPONSE_505 = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

// 当前服务器对应的socket
int server_socket;  
extern char **environ;

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

        // 构建文件路径映射
        char file_path[1024] = "./static_site";
        char *uri = request->http_uri;

        if(uri[0] == '/') uri++;
        if(strlen(uri) == 0) {
            strcat(file_path, "/index.html");  // file_path: "./static_site/index.html"
        }
        else {
            strcat(file_path, "/");  // file_path: "./static_site/"
            strcat(file_path, uri);  // file path: "./static_site/uri"
        }

        // CGI
        fprintf(stderr, "uri: %s\n", request->http_uri);
        if(strstr(uri, "cgi")){
            char *tmp;
            if((tmp = strchr(file_path, '?')) != NULL){
                *tmp = '\0'; 
            }
            fprintf(stderr, "uri: %s\n", request->http_uri);
            exec_cgi(client_sock, file_path, request, client_ip);
            return 0;
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
        return 1;
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
    // Server name
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

void send_error_500(int client_socket){
    char *err = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    send(client_socket, err, strlen(err), 0);
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

// 绑定空闲客户端
int bind_client_context(client_context client_set[], int client_sock, struct sockaddr_in cli_addr){
    int i;
    for(i = 0; i < FD_SETSIZE; i++){
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
    return i == FD_SETSIZE ? -1 : i;
}

// 执行cgi程序
void exec_cgi(int client_socket, char *file_name, Request *req, const char *client_ip){
    pid_t pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int readret;
    char cgi_buf[BUF_SIZE];
    int cgi_failed = 0;

    if(pipe(stdin_pipe)){
        fprintf(stderr, "Failed pipe the stdin_pipe.\n");
        return;
    }
    if(pipe(stdout_pipe)){
        fprintf(stderr, "Failed pipe the stdin_pipe.\n");
        return;
    }

    pid = fork();
    if(pid > 0){
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        // write(stdin_pipe[1], req, sizeof(req));  // 发送请求 -->> cgi子进程
        close(stdin_pipe[1]);        
        // fprintf(stdout, "CGI file path: %s\n", file_name);
        // fprintf(stdout, "Got from CGI:\n");
        while((readret = read(stdout_pipe[0], cgi_buf, BUF_SIZE - 1)) > 0){
            send(client_socket, cgi_buf, readret, 0);
            // 转发给客户端
            cgi_buf[readret] = '\0'; /* nul-terminate string */
            fprintf(stdout, "Got from CGI: %s\n", cgi_buf);
            fflush(stdout);
        }
        close(stdout_pipe[0]);
        
        int status;
        waitpid(pid, &status, 0);
        // 如果子进程异常退出，发送500错误
        if(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0)){
            fprintf(stderr, "CGI child terminated abnormally\n");
            send_error_500(client_socket);
        }
    }
    else if(pid == 0){
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], fileno(stdin));
        dup2(stdout_pipe[1], fileno(stdout));
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // 构建argv
        char *argv[] = { file_name, NULL};

        // 构建environ
        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
        setenv("SERVER_SOFTWARE", "Liso/1.0", 1);
        setenv("REQUEST_METHOD", req->http_method, 1);
        setenv("REQUEST_URI", req->http_uri, 1);
        
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", ECHO_PORT);
        setenv("SERVER_PORT", port_str, 1);
        setenv("REMOTE_ADDR", client_ip, 1);
        setenv("SCRIPT_NAME", file_name, 1);
        
        // QUERY_STRING（从 URI 中解析 ? 后面的部分）
        char *qmark = strchr(req->http_uri, '?');
        if(qmark != NULL){
            setenv("QUERY_STRING", qmark + 1, 1);
            fprintf(stderr, "query_string: %s\n", qmark + 1);
        } 
        else{
            setenv("QUERY_STRING", "", 1);
        }
        
        // 遍历请求头
        for(int i = 0; i < req->header_count; i++){
            char *name = req->headers[i].header_name;
            char *value = req->headers[i].header_value;
            if(strcmp(name, "Content-Length") == 0){
                setenv("CONTENT_LENGTH", value, 1);
            } 
            else if(strcmp(name, "Content-Type") == 0){
                setenv("CONTENT_TYPE", value, 1);
            }
            else{
                char env_name[256];
                snprintf(env_name, sizeof(env_name), "HTTP_%s", name);
                // 将 - 转换为 _
                for(char *p = env_name; *p; p++){
                    if(*p == '-') *p = '_';
                }
                // 转换为大写
                for(char *p = env_name; *p; p++){
                    if(*p >= 'a' && *p <= 'z') *p -= 32;
                }
                setenv(env_name, value, 1);
            }
        }

        execve(file_name, argv, environ);
        fprintf(stderr, "CGI execve failed: %s\n", file_name);
        exit(1);
    }
    else{
        fprintf(stderr, "CGI: fork failed\n");
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        send_error_500(client_socket);
        return;
    }
}