#include "log.h"

const char *ErrorLogDefault = "logs/error.log";  // 错误日志默认路径
const char *AccessLogDefault = "logs/access.log"; // 正确日志默认路径
FILE *errorFd = NULL;  //错误日志文件描述符
FILE *accessFd = NULL; //正确日志文件描述符

// 确保日志目录存在
static void ensure_log_dir() {
    struct stat st;
    if (stat("logs", &st) == -1) {
        mkdir("logs", 0755);  // 创建 logs 目录
    }
}

void init_log() {
    ensure_log_dir();
    // 以追加模式打开
    errorFd = fopen(ErrorLogDefault, "a");
    if(errorFd == NULL) {
        fprintf(stderr, "[Warning] %s\n", ErrorLogDefault);
        return ;
    }
    accessFd = fopen(AccessLogDefault, "a");
    if(accessFd == NULL) {
        fprintf(stderr, "[Warning] %s\n", AccessLogDefault);
        return ;
    }
}

void error_log(const char *IPaddr, const char *message, const char *detail) {
    if(errorFd == NULL) return ;

    // 错误信息
    char error_info[128];

    // 获取时间
    time_t now;
    struct tm *tm_info;
    char time_str[64];
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);

    // 获取IP addr
    char ipAddrStr[16] = "unknown";
    if(IPaddr != NULL) {
        strncpy(ipAddrStr, IPaddr, sizeof(ipAddrStr) - 1);
        ipAddrStr[15] = '\0';
    }

    // 组装error Information
    sprintf(error_info, "[%s] [error] [client %s] %s", time_str, ipAddrStr, 
        message == NULL ? "Unknown" : message);
    fprintf(errorFd, error_info);

    if(message != NULL && detail != NULL) {
        fprintf(errorFd, ": %s", detail);
    }
    fprintf(errorFd, "\n");

    // 清空
    fflush(errorFd);
    return ;
}

void access_log(const char* IPaddr, const char *request_line, int status, int content_length) {
    if(accessFd == NULL) return ;

    // 获取时间
    time_t now;
    struct tm *tm_info;
    char time_str[64];
    time(&now);
    tm_info = gmtime(&now);
    strftime(time_str, sizeof(time_str), "%d/%b/%Y:%H:%M:%S", tm_info);

    // 获取IP addr
    char ipAddrStr[16] = "unknown";
    if(IPaddr != NULL) {
        strncpy(ipAddrStr, IPaddr, sizeof(ipAddrStr) - 1);
        ipAddrStr[15] = '\0';
    }

    // 组装 access Information
    char access_info[128];
    sprintf(access_info, "%s - - [%s +0000] \"%s\" %d %d\n", ipAddrStr, time_str, request_line, 
        status, content_length);
    fprintf(accessFd, access_info);

    fflush(accessFd);
    return ;
}

void close_log() {
    if (errorFd != NULL) {
        fflush(errorFd);
        fclose(errorFd);
        errorFd = NULL;
    }
    if (accessFd != NULL) {
        fflush(accessFd);
        fclose(accessFd);
        accessFd = NULL;
    }
}