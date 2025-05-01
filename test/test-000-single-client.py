#!/usr/bin/env python3
#
# test-000-single-client.py – basic single-client interaction test for wd-broker.
#
# This test registers a single client, sends periodic heartbeats (PINGs) 
# within the allowed timeout window, and then unregisters the client cleanly.
# It verifies that wd-broker correctly handles the normal client lifecycle.
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

broker = TestBroker()
broker.start()

clientID = register("testclient")

for i in range(5):
    time.sleep(1)
    check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")

unregister(clientID)

broker.stop()
