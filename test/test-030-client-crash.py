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
# Copyright (C) 2025 Julian Friedrich
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# This file is part of the wd-broker project:
#   https://github.com/fjulian79/wd-broker
#
# Please feel free to open issues or contribute improvements.

from common import *
import time
import sys
import os

broker = TestBroker()
broker.start()

clients = []
names = ["alpha", "beta", "alpha", "gamma", "delta", "beta"]
log_path = os.environ.get("BROKER_LOG")

# Register all clients
for name in names:
    clientID = register(name, 3000)
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

log_info(f"Adding delay to the broker detect the timeout of {dead_name} ({dead_id})")
time.sleep(2)

# now expecting the surviving clients to see errors as the broker should be down.
for name, clientID in clients:
    check_cmd(f"PING {name}({clientID})", f"PING {clientID}\n", shall_fail=True)

log_info("Checking broker log for timeout message...")
if not os.path.exists(log_path):
    fail(f"Expected broker log file '{log_path}' not found.")
with open(log_path) as f:
    log = f.read()

if "CLIENT HEARTBEAT TIMEOUT OCCURED" not in log:
    print(log)
    fail("Expected timeout message not found in broker log.")
log_step("Timeout message confirmed in log.")

broker.stop()
