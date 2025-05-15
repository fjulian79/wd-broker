#!/usr/bin/env python3
#
# client-sim.py – Simulates multiple concurrent wd-broker clients with PING/UNREGISTER behavior.
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
import argparse

DEFAULT_SOCKET_PATH = "/run/wd-broker.sock"

tclients = []
stop_event = threading.Event()

class ClientThread(threading.Thread):
    def __init__(self, socket_path, name, timeout_ms=5000, ignorepid=False):
        super().__init__()
        self.socket_path = socket_path
        self.name = name
        self.timeout_ms = timeout_ms
        self.ignorepid = ignorepid
        self.client_id = None
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)

    def run(self):
        try:
            self.sock.connect(self.socket_path)
            ignore_flag = "ignorepid" if self.ignorepid else ""
            register_cmd = f"REGISTER {self.name} {self.timeout_ms} {ignore_flag}".strip() + "\n"
            self.sock.sendall(register_cmd.encode())
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
                with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as s:
                    s.connect(self.socket_path)
                    s.sendall(f"PING {self.client_id}\n".encode())
                    response = s.recv(1024).decode().strip()
                    print(f"[{self.name}] Ping: {response}")
            except Exception as e:
                print(f"[{self.name}] Ping failed: {e}")
            time.sleep(self.timeout_ms / 2000.0)

        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as s:
                s.connect(self.socket_path)
                s.sendall(f"UNREGISTER {self.client_id}\n".encode())
                _ = s.recv(1024)
                print(f"[{self.name}] Unregistered")
        except Exception as e:
            print(f"[{self.name}] Failed to unregister: {e}")

def signal_handler(sig, frame):
    print("\nStopping clients...")
    stop_event.set()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Simulate multiple wd-broker clients.")
    parser.add_argument("--socket-path", default=DEFAULT_SOCKET_PATH, help="Path to broker UNIX domain socket (default: " + DEFAULT_SOCKET_PATH + ")")
    args = parser.parse_args()

    signal.signal(signal.SIGINT, signal_handler)

    client_definitions = [
        ("alpha", 5000, False),
        ("beta", 6000, False),
        ("gamma", 7000, False),
        ("delta", 8000, True),
        ("alpha", 5000, False),
    ]

    clients = []
    for name, timeout_ms, ignorepid in client_definitions:
        t = ClientThread(args.socket_path, name, timeout_ms, ignorepid)
        clients.append(t)
        t.start()

    for t in clients:
        t.join()

    print("All clients stopped.")
