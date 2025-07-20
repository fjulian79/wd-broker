#!/usr/bin/env python3
#
# test-022-announced-ok.py – test announced client registration and heartbeat
#
# This test registers several clients while onf them is announced via the
# config file. It checks that the broker logs the registration message
# correctly, including the PID of the announced client. The test also
# verifies that the announced client can send heartbeats (PINGs) and
# receives the expected response.
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
import os
import re

broker = TestBroker()
broker.start(config="cfg/announced-beta-5500.conf")

clients = []
names = ["alpha", "beta", "gamma", "delta"]

# Register all clients
for name in names:
    clientID = register(name)
    clients.append((name, clientID))

# Send heartbeat (PING) from all clients
for i in range(7):
    for name, clientID in clients:
        check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", expect="OK")
    time.sleep(1)  # Wait a second before the next round of PINGs

log_info("Checking broker log for register message ...")
log = broker.get_log()

# Check for "nnounced client 'beta' (PID 178248) registered" but use a placeholder regex for the PID
if not any(re.search(r"Announced client 'beta' \(PID \d+\) registered", line) for line in log):
    for line in log:
        print("[BROKER]", line)
    fail("Expected register message not found in broker log.")
log_step("Register message confirmed in log.")

broker.stop()
