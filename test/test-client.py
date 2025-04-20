#!/usr/bin/env python3

import socket, time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/wd-broker.sock")
sock.sendall(b"REGISTER testclient 3000\n")
clientID = sock.recv(128).decode().strip().split()[1]
print("Got clientID:", clientID)

# Ping
for i in range(5):
    time.sleep(1)
    sock2 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock2.connect("/tmp/wd-broker.sock")
    sock2.sendall(f"PING {clientID}\n".encode())
    print("Reply:", sock2.recv(128).decode().strip())
    sock2.close()

# Disconnect
sock3 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock3.connect("/tmp/wd-broker.sock")
sock3.sendall(f"UNREGISTER {clientID}\n".encode())
print("Unregister:", sock3.recv(128).decode().strip())
