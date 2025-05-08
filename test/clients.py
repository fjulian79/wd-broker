#!/usr/bin/env python3
#
# clients.py – Simulates multiple concurrent wd-broker clients with PING/UNREGISTER behavior.
#
# Spawns multiple named clients, each registering with the broker and periodically sending
# PINGs until interrupted. Also tests the 'ignorepid' registration flag in mixed scenarios.
#
# Useful for manual inspection, stress testing, and protocol behavior validation.
#
#     Copyright 2025 Julian Friedrich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is provided on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Source repository: https://github.com/fjulian79/wd-broker

import socket
import time
import os
import threading
import signal
import sys

SOCKET_PATH = "/run/wd-broker.sock"
PING_INTERVAL = 1.0  # Sekunden
TIMEOUT_MS = 5000

clients = []
stop_event = threading.Event()

class ClientThread(threading.Thread):
    def __init__(self, name, ignorepid=False):
        super().__init__()
        self.name = name
        self.ignorepid = ignorepid
        self.client_id = None
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    def run(self):
        try:
            self.sock.connect(SOCKET_PATH)
            ignore_flag = "ignorepid" if self.ignorepid else ""
            self.sock.sendall(f"REGISTER {self.name} {TIMEOUT_MS} {ignore_flag}\n".encode())
            response = self.sock.recv(1024).decode().strip()
            if not response.startswith("OK "):
                print(f"[{self.name}] Failed to register: {response}")
                return
            self.client_id = response.split()[1]
            print(f"[{self.name}] Registered with ID {self.client_id}")
            self.sock.close()
        except Exception as e:
            print(f"[{self.name}] Exception during register: {e}")
            return

        while not stop_event.is_set():
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                    s.connect(SOCKET_PATH)
                    s.sendall(f"PING {self.client_id}\n".encode())
                    _ = s.recv(1024)
            except Exception as e:
                print(f"[{self.name}] Ping failed: {e}")
            time.sleep(PING_INTERVAL)

        # Unregister on stop
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.connect(SOCKET_PATH)
                s.sendall(f"UNREGISTER {self.client_id}\n".encode())
                _ = s.recv(1024)
                print(f"[{self.name}] Unregistered")
        except Exception as e:
            print(f"[{self.name}] Failed to unregister: {e}")

def signal_handler(sig, frame):
    print("\nStopping clients...")
    stop_event.set()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    # Definition mit ignorepid pro Client
    client_definitions = [
        ("alpha", False),
        ("beta", False),
        ("gamma", False),
        ("delta", True),
        ("alpha", False),
    ]

    clients = []
    for name, ignorepid in client_definitions:
        t = ClientThread(name, ignorepid)
        clients.append(t)
        t.start()

    for t in clients:
        t.join()

    print("All clients stopped.")
