#!/usr/bin/env python3
#
# test-030-client-crash.py – simulate ungraceful client failure and validate broker reaction
#
# This test registers several clients and simulates one client crashing by 
# removing it from the heartbeat cycle without sending an UNREGISTER command.
# The broker is expected to detect the missing heartbeat, log a timeout message,
# and enter its passive state. The remaining clients will experience connection failure, 
# which is verified as part of the test. The test also checks that the expected timeout 
# message appears in the broker log.
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
broker.start()

clients = []
names = ["alpha", "beta", "alpha", "gamma", "delta", "beta"]

# Register all clients
for name in names:
    clientID = register(name)
    clients.append((name, clientID))

# Send heartbeat (PING) from all clients
for i in range(3):
    for name, clientID in clients:
        check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", expect="OK")
        time.sleep(0.2)  # Simulate a small delay between PINGs
    time.sleep(1)  # Wait a second before the next round of PINGs

dead_client_index = 2 # the second "alpha" client
dead_name, dead_id = clients.pop(dead_client_index)
log_info(f"Simulating crash of client {dead_name} ({dead_id})")

# one more ping of the surviving clients expected to succeed
for name, clientID in clients:
    check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", expect="OK")
    time.sleep(0.2)  # Simulate a small delay between PINGs

log_info(f"Adding delay to allow broker to detect timeout of {dead_name} ({dead_id})")
time.sleep(15)

# now expecting the surviving clients to see errors as the broker should be down.
for name, clientID in clients:
    check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", shall_fail=True)

log_info("Checking broker log for timeout message...")
log = broker.get_log()

if not any("SYSTEM RESET PENDING!" in line for line in log):
    for line in log:
        print("[BROKER]", line)
    fail("Expected timeout message not found in broker log.")
log_step("Timeout message confirmed in log.")

broker.stop()
