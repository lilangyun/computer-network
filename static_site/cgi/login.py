#!/usr/bin/env python3
import os
import urllib.parse

# 获取 QUERY_STRING
query_string = os.environ.get('QUERY_STRING', '')

# 解析参数
params = urllib.parse.parse_qs(query_string)

# 获取参数值（parse_qs 返回的是列表，取第一个元素）
username = params.get('username', [''])[0]
password = params.get('password', [''])[0]

# URL 解码（处理中文）
username = urllib.parse.unquote(username)
password = urllib.parse.unquote(password)

# 输出 HTTP 响应
print("HTTP/1.1 200 OK")
print("Content-Type: text/html; charset=utf-8")
print()

print(f"""<!DOCTYPE html>
<html>
<head><meta charset='utf-8'><title>登录信息</title></head>
<body>
<h1>您提交的信息</h1>
<p><strong>账号：</strong> {username}</p>
<p><strong>密码：</strong> {password}</p>
<br>
<a href='/login.html'>返回登录页面</a>
</body>
</html>""")
