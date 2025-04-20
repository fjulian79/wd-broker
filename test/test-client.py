#!/usr/bin/env python3

import socket, time

SOCKET_PATH = "/tmp/wd-broker-test.sock"

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(SOCKET_PATH)
sock.sendall(b"REGISTER testclient 3000\n")
clientID = sock.recv(128).decode().strip().split()[1]
print("Got clientID:", clientID)

# Ping
for i in range(5):
    time.sleep(1)
    sock2 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock2.connect(SOCKET_PATH)
    sock2.sendall(f"PING {clientID}\n".encode())
    print("Reply:", sock2.recv(128).decode().strip())
    sock2.close()

# Disconnect
sock3 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock3.connect(SOCKET_PATH)
sock3.sendall(f"UNREGISTER {clientID}\n".encode())
print("Unregister:", sock3.recv(128).decode().strip())
