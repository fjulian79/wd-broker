-0

#!/usr/bin/env python3
#
# test-024-unique-strict-clients.py – verify broker enforces unique client names
# when configured with unique_clients=true and does only allow clients which 
# have been announced in the configuration file.
#
# This test starts wd-broker with a configuration that enables unique and strict
# client names. It registers the set of announced clients successfully,
# then attempts to register some more client names but all should fail. 
# After some pings the registered clients are unregistered. But this should also
# fail becuase of the strict_clients setting.
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
broker.start(config="cfg/unique-strict-clients.conf")

client_grp1 = ["alpha", "beta", "gamma"]
client_grp2 = ["delta", "epsilon", "zeta", "alpha1", "alpha2"]
clients = []

# Register first group of clients, these are expected to succeed
for name in client_grp1:
    clientID = register(name)
    clients.append((name, clientID))

# Attempt to register the first group of clients again, this should fail now
for name in client_grp1:
    clientID = register(name, expect="ERROR")

# Register second group of clients which should also fail
for name in client_grp2:
    clientID = register(name, expect="ERROR")

# Send heartbeat (PING) from all clients
for i in range(5):
    for name, clientID in clients:
        check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")
    time.sleep(1)
    
# Unregister all clients
for name, clientID in clients:
    unregister(clientID, expect="ERROR")

broker.stop()