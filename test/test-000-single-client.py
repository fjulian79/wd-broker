#!/usr/bin/env python3
#
# test-000-single-client.py – Basic single-client interaction test for wd-broker.
#
# This test registers a single client, sends periodic heartbeats (PINGs)
# within the allowed timeout window, and then unregisters the client cleanly.
# It verifies that wd-broker correctly handles the normal client lifecycle.
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

clientID = register("testclient")

for i in range(5):
    time.sleep(1)
    check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")

unregister(clientID)

broker.stop()
