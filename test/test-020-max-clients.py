#!/usr/bin/env python3
#
# test-020-max-clients.py – test client registration limit and reuse behavior in wd-broker
#
# This test verifies that wd-broker enforces the maximum number of allowed clients,
# rejects further registrations once the limit is reached, and allows registration
# again after a client is properly unregistered. All registered client IDs must be
# unique, and re-registration must result in a new unique ID (not recycled).
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
import sys

broker = TestBroker()
broker.start()

clients = []
ids = set()

# Register WD_MAX_CLIENTS clients
for i in range(WD_MAX_CLIENTS):
    clientID = register(f"client{i}", expect="OK")
    if clientID in ids:
        fail(f"Duplicate clientID detected: {clientID}")
    ids.add(clientID)
    clients.append((f"client{i}", clientID))

log_step("All clientIDs are unique.")

# Register another client (65th), this should fail
log_info(f"Attempting to register a {WD_MAX_CLIENTS + 1}th client — must fail")
# register will abort if it doesn't get the error
register("overflow", expect="ERROR")

# Unregister a client in the middle
idx = WD_MAX_CLIENTS // 2
clientName, clientID = clients[idx]
unregister(clientID, name=clientName)
clients.pop(idx)

# Register a new client after one has unregistered
log_info("Registering new client after one was unregistered")
clientID = register("replacement", expect="OK")
if clientID in ids:
    fail(f"Duplicate clientID detected: {clientID}")
log_step("New client received a new ID")
clients.append(("replacement", clientID))

# Cleanup
for name, clientID in clients:
    unregister(clientID, name=name)

broker.stop()
