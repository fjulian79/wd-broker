#!/usr/bin/env python3
#
# test-031-register-timeout.py – simulate the timeout of a announced client
#
# This test registers several clients but not the one announced via the
# config file. It checks that the broker logs the timeout message because of the 
# missing client. The test also verifies that the announced client can send
# heartbeats (PINGs) and receives the expected response as long as the broker is
# alive. After the timeout, the broker should not acknowledge the PINGs anymore 
# and should log a message about the timeout and the upcoming system reset.
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

broker = TestBroker()
broker.start(config="cfg/announced-beta-5500.conf")

clients = []
names = ["alpha", "gamma", "delta"]

# Register all clients
for name in names:
    clientID = register(name)
    clients.append((name, clientID))

# Send heartbeat (PING) from all clients
for i in range(5):
    for name, clientID in clients:
        check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", expect="OK")
    time.sleep(1)  # Wait a second before the next round of PINGs

# Wait a little bit, the broker will detect missing client beta in this time
time.sleep(1)

# now expecting the surviving clients to see errors as the broker should be down.
for name, clientID in clients:
    check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", shall_fail=True)

log_info("Checking broker log for timeout messages ...")
log = broker.get_log()

if not any("Client 'beta' not registered within" in line for line in log):
    for line in log:
        print("[BROKER]", line)
    fail("Expected timeout message not found in broker log.")
log_step("Timeout message confirmed in log.")

if not any("SYSTEM RESET PENDING!" in line for line in log):
    for line in log:
        print("[BROKER]", line)
    fail("Expected system reset message not found in broker log.")
log_step("System reset message confirmed in log.")

broker.stop()
