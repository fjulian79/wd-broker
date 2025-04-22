#!/usr/bin/env python3
#
# test-multiple-clients.py – basic multi-client interaction test for wd-broker
#
# This test registers multiple clients with identical and distinct names, 
# sends periodic heartbeats (PINGs) to simulate normal activity, and finally
# unregisters all clients. It validates that wd-broker handles multiple clients
# concurrently and accepts duplicate names without conflict.
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

clients = []
names = ["alpha", "beta", "alpha", "gamma", "delta", "beta"]

# Register all clients
for name in names:
    clientID = register(name, 3000)
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

log_info("All clients successfully registered, pinged and unregistered.")
sys.exit(0)
