#!/usr/bin/env python3
import socket
import time

SOCKET_PATH = "/tmp/wd-broker.sock"

def send(cmd):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(SOCKET_PATH)
        sock.sendall(cmd.encode())
        return sock.recv(128).decode().strip()

# Registrieren mit kurzem Timeout (2 Sekunden)
reply = send("REGISTER lazyclient 2000\n")
uid = reply.split()[1]
print("Registered as:", uid)

# Einmal PING (dann nichts mehr)
print("Sending 1st PING...")
print("Reply:", send(f"PING {uid}\n"))

# Nun WARTEN bis Timeout überschritten ist
print("Waiting 5 seconds to trigger timeout...")
time.sleep(5)

# Dann nochmal PING – sollte dennoch akzeptiert werden (Broker bleibt leise)
print("Sending late PING...")
print("Reply:", send(f"PING {uid}\n"))

# Zum Abschluss abmelden
print("Unregistering...")
print("Reply:", send(f"UNREGISTER {uid}\n"))
