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

import os
import socket
import time

from common import *

def create_regular_file(path=SOCKET_PATH):
    with open(path, "w") as f:
        f.write("not a socket")

def create_active_socket(path=SOCKET_PATH):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    sock.bind(path)
    sock.listen(1)
    return sock

def create_stale_socket(path=SOCKET_PATH):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
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

clientID = register("testclient", 3000)
for i in range(5):
    time.sleep(0.2)
    check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")
unregister(clientID)

broker.stop()
