#!/usr/bin/env python3
#
# test-020-max-clients.py – test client registration limit and reuse behavior in wd-broker
#
# This test verifies that wd-broker enforces the maximum number of allowed clients,
# rejects further registrations once the limit is reached, and allows registration
# again after a client is properly unregistered. All registered client IDs must be
# unique, and re-registration must result in a new unique ID (not recycled).
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
import sys

broker = TestBroker()
broker.start()

heartbeat_ms = 30000
clients = []
ids = set()

# Register MAX_CLIENTS clients
for i in range(MAX_CLIENTS):
    clientID = register(f"client{i}", timeout_ms=heartbeat_ms, expect="OK")
    if clientID in ids:
        fail(f"Duplicate clientID detected: {clientID}")
    ids.add(clientID)
    clients.append((f"client{i}", clientID))

log_step("All clientIDs are unique.")

# Register another client (65th), this should fail
log_info(f"Attempting to register a {MAX_CLIENTS + 1}th client — must fail")
# register will abort if it doesn't get the error
register("overflow", timeout_ms=heartbeat_ms, expect="ERROR")

# Unregister a client in the middle
idx = MAX_CLIENTS // 2
clientName, clientID = clients[idx]
unregister(clientID, name=clientName)
clients.pop(idx)

# Register a new client after one has unregistered
log_info("Registering new client after one was unregistered")
clientID = register("replacement", timeout_ms=heartbeat_ms, expect="OK")
if clientID in ids:
    fail(f"Duplicate clientID detected: {clientID}")
log_step("New client received a new ID")
clients.append(("replacement", clientID))

# Cleanup
for name, clientID in clients:
    unregister(clientID, name=name)

broker.stop()
