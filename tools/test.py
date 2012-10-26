#python2.7.3
#coding=utf-8

import socket
import time
import threading #fuck the threading module

http_request = "POST /test_server HTTP/1.1\r\nHost:test.py\r\nContent-Length:5\r\n\r\nHello"

def make_a_request():
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.connect(('localhost', 10000))
    sockfd.sendall(http_request)
    sockfd.recv(10000)
    sockfd.close()

if __name__ == "__main__":
    thread_list = []
        
    start_time = time.time()

    for i in range(0, 1000):
        thread = threading.Thread(target = make_a_request)
        thread_list.append(thread)
        thread.start()

    for thread in thread_list:
        thread.join()
    
    #fuck the python threading module
    print "Time used for 1000 request: ", time.time() - start_time
