#!/bin/bash
echo "HTTP/1.1 200 OK"
echo "Content-Type: text/html; charset=utf-8"
echo ""

# 手动解析 QUERY_STRING
USERNAME=$(echo "$QUERY_STRING" | sed 's/.*username=\([^&]*\).*/\1/')
PASSWORD=$(echo "$QUERY_STRING" | sed 's/.*password=\([^&]*\).*/\1/')

echo "<!DOCTYPE html>"
echo "<html>"
echo "<head><meta charset='utf-8'><title>登录信息</title></head>"
echo "<body>"
echo "<h1>您提交的信息</h1>"
echo "<p><strong>账号：</strong> $USERNAME</p>"
echo "<p><strong>密码：</strong> $PASSWORD</p>"
echo "<br>"
echo "<a href='/login.html'>返回登录页面</a>"
echo "</body>"
echo "</html>"
