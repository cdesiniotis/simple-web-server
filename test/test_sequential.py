import sys
import os
import socket


def create_connection(ip, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((ip, port))
    except (ConnectionRefusedError, OSError):
        print("[ERROR] Error connecting to manager\n", file=sys.stderr)
        sys.exit(1)
    print("Successfully connected to server")
    return s

ip = "127.0.0.1"
port = 8000
bufsize = 4096

requests = [
    # Status code 200
    "GET /hello.html HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port),
    # Status code 400
    "GET HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port),
    # Status code 403
    "GET /forbidden.html HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port),
    # Status code 404
    "GET /notfound.html HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port),
    # Status code 503
    "POST /index.html HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port)
]


for request in requests:
    s = create_connection(ip,port)
    print("Sending request: \n{}".format(request))
    s.send(request.encode())
    while True:
        buf = s.recv(bufsize)
        if (len(buf) is 0):
            break
        print(buf.decode())
