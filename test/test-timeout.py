#!/usr/bin/env python3
import socket
import time

SOCKET_PATH = "/tmp/wd-broker-test.sock"

def send(cmd):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(SOCKET_PATH)
        sock.sendall(cmd.encode())
        return sock.recv(128).decode().strip()

reply = send("REGISTER lazyclient 2000\n")
clientID = reply.split()[1]
print("Registered as:", clientID)

print("Sending 1st PING...")
print("Reply:", send(f"PING {clientID}\n"))

print("Waiting 5 seconds to trigger timeout...")
time.sleep(5)

print("Sending late PING...")
print("Reply:", send(f"PING {clientID}\n"))

print("Unregistering...")
print("Reply:", send(f"UNREGISTER {clientID}\n"))
