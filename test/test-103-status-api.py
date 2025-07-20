
-0

#!/usr/bin/env python3
#
# test-103-status-api.py – Protocol compliance test for wd-broker STATUS command.
#
# Verifies that wd-broker returns expected status output when queried by root
# and rejects malformed STATUS requests.
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
import os
import re

# if not running as root, skip the test
if not is_root():
    skip("This test must be run as root to access the STATUS API.")

broker = TestBroker()
broker.start()

client1 = register("status-alpha")
client2 = register("status-beta")

reply = send_socket_command("STATUS\n")
lines = [line.strip() for line in reply.splitlines() if line.strip()]

expected_fields = [
    "daemon_version=" + PACKAGE_VERSION,
    "protocol_version=" + SOCKET_PROT_VERSION,
    "wd_timeout_s=10",
    "strict_clients=false",
    "unique_clients=false",
    "active_clients=2",
]

for field in expected_fields:
    if not any(line.startswith(field) for line in lines):
        fail(f"Missing field '{field}' in STATUS reply: {reply}")

pid = os.getpid()
pattern = re.compile(rf"{client1} {pid} status-alpha {DEFAULT_TIMEOUT} 1")
if not any(pattern.match(l) for l in lines):
    fail("Client entry for status-alpha not found in STATUS output")

pattern = re.compile(rf"{client2} {pid} status-beta {DEFAULT_TIMEOUT} 1")
if not any(pattern.match(l) for l in lines):
    fail("Client entry for status-beta not found in STATUS output")

error_reply = send_socket_command("STATUS junk\n")
if not error_reply.startswith("ERROR"):
    fail(f"Malformed STATUS command not rejected: {error_reply}")

unregister(client1)
unregister(client2)

broker.stop()