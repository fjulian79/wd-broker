#!/usr/bin/env python3
import socket
import sys

SOCKET_PATH = "/tmp/wd-broker-test.sock"

def fail(msg):
    print("[ERROR]", msg, file=sys.stderr)
    sys.exit(1)

def send_cmd(cmd, expect=None):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(SOCKET_PATH)
        sock.sendall(cmd.encode())
        reply = sock.recv(128).decode().strip()
        if expect and not reply.startswith(expect):
            fail(f"Expected '{expect}', got: '{reply}'")
        return reply

def register(name, timeout_ms):
    reply = send_cmd(f"REGISTER {name} {timeout_ms}\\n", expect="OK")
    clientID = reply.split()[1]
    print(f"[INFO] Registered client \"{name}\", clientID: {clientID}")
    return clientID

def unregister(clientID):
    send_cmd(f"UNREGISTER {clientID}\n", expect="OK")
