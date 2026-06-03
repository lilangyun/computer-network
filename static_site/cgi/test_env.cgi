#!/bin/bash

# 输出HTTP响应头
echo "HTTP/1.1 200 OK"
echo "Content-Type: text/plain; charset=utf-8"
echo ""

# 输出环境变量
echo "=== CGI Environment Variables ==="
echo ""
echo "QUERY_STRING=$QUERY_STRING"
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "REQUEST_URI=$REQUEST_URI"
echo "SCRIPT_NAME=$SCRIPT_NAME"
echo "SERVER_PORT=$SERVER_PORT"
echo "REMOTE_ADDR=$REMOTE_ADDR"
echo "GATEWAY_INTERFACE=$GATEWAY_INTERFACE"
echo "SERVER_PROTOCOL=$SERVER_PROTOCOL"
echo "SERVER_SOFTWARE=$SERVER_SOFTWARE"
echo "CONTENT_LENGTH=$CONTENT_LENGTH"
echo ""

echo "=== HTTP Headers ==="
env | grep HTTP_ | sort
