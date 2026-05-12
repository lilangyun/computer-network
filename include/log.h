#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

void init_log();
void error_log(const char *IPaddr, const char *message, const char *detail); // 错误日志模块
void access_log(const char* IPaddr, const char *request_line, int status, int content_length);  // 通过日志模块
void close_log();