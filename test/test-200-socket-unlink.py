#!/usr/bin/env python3
#
# test-200-socket-unlink.py – robustness test for socket handling during broker startup
#
# This test verifies that wd-broker correctly handles various scenarios regarding
# the presence and validity of the UNIX socket at startup:
#   - No socket file present (normal startup)
#   - Regular file present instead of a socket (should fail)
#   - Active socket already in use (should fail)
#   - Stale socket file without a running server (should clean up and start)
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

import os
import socket
import time

from common import *

def create_regular_file(path=SOCKET_PATH):
    with open(path, "w") as f:
        f.write("not a socket")

def create_active_socket(path=SOCKET_PATH):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    sock.bind(path)
    sock.listen(1)
    return sock

def create_stale_socket(path=SOCKET_PATH):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    sock.bind(path)
    sock.close()

def clean_up(path=SOCKET_PATH):
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass

clean_up()
log_info("Scenario 1: socket does not exist")
broker = TestBroker()
if not broker.start(expect="OK"):
    fail("Broker should have started successfully (no socket existed)")
broker.stop()

clean_up()
log_info("Scenario 2: regular file exists instead of socket")
create_regular_file()
broker = TestBroker()
if not broker.start(expect="FAIL"):
    fail("Broker should have failed to start with regular file")
    
clean_up()
log_info("Scenario 3: active socket (already in use)")
active_sock = create_active_socket()
broker = TestBroker()
if not broker.start(expect="FAIL"):
    fail("Broker should have failed to start with active socket")
active_sock.close()

clean_up()
log_info("Scenario 4: stale socket (no active server)")
create_stale_socket()
broker = TestBroker()
if not broker.start(expect="OK"):
    fail("Broker should have started successfully after removing stale socket")

clientID = register("testclient")
for i in range(5):
    time.sleep(0.2)
    check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")
unregister(clientID)

broker.stop()
