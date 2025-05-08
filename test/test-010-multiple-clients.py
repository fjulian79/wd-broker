#!/usr/bin/env python3
#
# test-multiple-clients.py – basic multi-client interaction test for wd-broker
#
# This test registers multiple clients with identical and distinct names, 
# sends periodic heartbeats (PINGs) to simulate normal activity, and finally
# unregisters all clients. It validates that wd-broker handles multiple clients
# concurrently and accepts duplicate names without conflict.
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

from common import *
import time
import sys

broker = TestBroker()
broker.start()

clients = []
names = ["alpha", "beta", "alpha", "gamma", "delta", "beta"]

# Register all clients
for name in names:
    clientID = register(name)
    clients.append((name, clientID))

# Send heartbeat (PING) from all clients
for i in range(5):
    for name, clientID in clients:
        check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")
        time.sleep(0.2)  # Simulate a small delay between PINGs
    time.sleep(1)  # Wait a second before the next round of PINGs
    
# Unregister all clients
for name, clientID in clients:
    unregister(clientID)

broker.stop()
