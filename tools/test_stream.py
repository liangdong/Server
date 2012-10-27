#python2.7.3
#coding=utf-8

import socket

http_request = "POST /test_server HTTP/1.1\r\nHost:test.py\r\nContent-Length:5\r\n\r\nHello"

def make_a_request():
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.connect(('localhost', 10000))
    for i in range(0, 5):
        sockfd.sendall(http_request)
        print sockfd.recv(10000)
    sockfd.close()

if __name__ == "__main__":
    make_a_request()
