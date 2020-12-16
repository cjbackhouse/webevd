#!/usr/bin/env python

import os
import socket

def startup(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Reuse port immediately even if a previous instance just aborted.
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', port))
    # Parameter is the length of "backlog" allowed
    s.listen(128)
    return s


def is_complete_query(msg):
    # headers are always terminated by \r\n\r\n
    kTerminator = b'\r\n\r\n'

    if kTerminator not in msg: return False
    
    headerSize = msg.find(kTerminator)+4

    cl = b'Content-Length: '
    # No content-length, therefore no body
    if cl not in msg: return True
    lpos = msg.find(cl)

    # Parse the context-length header
    newline = msg.find(b'\r', lpos+len(cl))
    bodyLength = int(msg[lpos+len(cl):newline])

    return len(msg) == headerSize+bodyLength


def ok200_html():
    return 'HTTP/1.0 200 OK\r\n'\
        'Server: WebEVDBrowser/0.1.0\r\n'\
        'Content-Type: text/html\r\n'\
        '\r\n'

def unimp501():
    return 'HTTP/1.0 501 Not Implemented\r\n'\
        'Server: WebEVDBrowser/0.1.0\r\n'\
         'Content-Type: text/plain\r\n'\
         '\r\n'\
         "I don't know how to do that\r\n"

def send(sock, msg):
    sock.send(msg.encode('utf-8'))

def serve(sock, path):
    send(sock, ok200_html())

    send(sock, '<!DOCTYPE html><html><head><meta charset="utf-8"><title>'+path+'</title></head><body><h1>'+path+'</h1>\n')

    files = []
    dirs = [os.path.normpath(os.path.join(path, '..'))]

    for f in os.listdir(path):
        fullpath = os.path.join(path, f)
        if os.path.isfile(fullpath):
            files += [fullpath]
        else:
            dirs += [fullpath]

    for d in sorted(dirs):
        send(sock, '<a href="'+d+'">'+os.path.relpath(d, path)+'</a><br>\n')

    send(sock, '<hr>\n')

    for f in sorted(files):
        if f[-5:] == '.root':
            send(sock, '<a href="'+f+'">'+os.path.relpath(f, path)+'</a><br>\n')
        else:
            send(sock, os.path.relpath(f, path)+'<br>\n')

    send(sock, '</body></html>\n')
    sock.close()


def error(sock, req):
    send(sock, unimp501())
    sock.send(req)
    sock.close()


if __name__ == '__main__':
    port = 1091

    sock = startup(port)

    user = os.getlogin()
    host = socket.gethostname()

    print('First run\nssh -L '+str(port)+':localhost:'+str(port)+" "+user+'@'+host+'\n\nand then navigate to localhost:'+str(port)+' in your favorite browser.')

    first = True

    while True:
        clientSock, addr = sock.accept()

        x = b''
        while True:
            x += clientSock.recv(4096)
            if is_complete_query(x): break

        if x[:4] == b'GET ':
            path = x.split(b' ')[1].decode('utf-8')

            if path == '/' and first: path = os.getcwd()

            serve(clientSock, path)
        else:
            error(clientSock, x)

        first = False
