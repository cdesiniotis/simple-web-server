import sys
import os
import socket
import argparse
import logging
import threading
import time

def create_connection(ip, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((ip, port))
    except (ConnectionRefusedError, OSError):
        print("[ERROR] Error connecting to manager\n", file=sys.stderr)
        sys.exit(1)
    print("Successfully connected to server")
    return s


class ReadThread(threading.Thread):
    def __init__(self, name, sockobj):
        super().__init__()
        self.sockobj = sockobj
        self.name = name
        self.bufsize = 4096

    def run(self):
        while True:
            buf = self.sockobj.recv(self.bufsize)
            if (len(buf) is 0):
                break
            logging.info('{0} received: \n{1}'.format(self.name, buf.decode()))

def make_new_connection(name, ip, port):
    """Creates a single socket connection to the host:port.
    """
    sockobj = create_connection(ip, port)
    logging.info('{0} connected...'.format(name))

    rthread = ReadThread(name, sockobj)
    rthread.start()

    request = "GET /hello.html HTTP/1.0\r\nHOST: {}:{}\r\n\r\n".format(ip, port)
    logging.info('{0} sending:\n{1}'.format(name, request))
    sockobj.send(request.encode())

    rthread.join()
    sockobj.close()
    logging.info('{0} disconnecting'.format(name))

            
def main():
    argparser = argparse.ArgumentParser('Concurrent HTTP client')
    argparser.add_argument('ip', help='server ip')
    argparser.add_argument('port', type=int, help='server port')
    argparser.add_argument('-n', '--num_concurrent', type=int,
                           default=1,
                           help='number of concurrent connections')
    args = argparser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG,
        format='%(levelname)s:%(asctime)s:%(message)s')

    t1 = time.time()
    connections = []
    for i in range(args.num_concurrent):
        name = 'conn{0}'.format(i)
        tconn = threading.Thread(target=make_new_connection,
                                 args=(name, args.ip, args.port))
        tconn.start()
        connections.append(tconn)

    for conn in connections:
        conn.join()

    print('Elapsed:', time.time() - t1)


if __name__ == '__main__':
    main()
