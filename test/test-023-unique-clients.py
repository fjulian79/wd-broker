-0

#!/usr/bin/env python3
#
# test-023-unique-clients.py – verify broker enforces unique client names
# when configured with unique_clients=true
#
# This test starts wd-broker with a configuration that enables
# unique client names. It registers a client "alpha" successfully,
# then attempts to register another client with the same name,
# which must be rejected. A different client name should still
# be accepted normally.
#
# Copyright 2025 Julian Friedrich
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

broker = TestBroker()
broker.start(config="cfg/unique-clients.conf")

client_grp1 = ["alpha", "beta"]
client_grp2 = ["gamma", "delta"]
clients = []

# Register first group of clients
for name in client_grp1:
    clientID = register(name)
    clients.append((name, clientID))

# Attempt to register the first group of clients again, this should fail
for name in client_grp1:
    clientID = register(name, expect="ERROR")

# Register second group of clients
for name in client_grp2:
    clientID = register(name)
    clients.append((name, clientID))

# Send heartbeat (PING) from all clients
for i in range(5):
    for name, clientID in clients:
        check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")
    time.sleep(1)  # Wait a second before the next round of PINGs
    
# Unregister all clients
for name, clientID in clients:
    unregister(clientID)

broker.stop()