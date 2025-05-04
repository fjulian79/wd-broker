#!/usr/bin/env python3
#
# test-002-ignorepid-client.py – Basic single-client muliprocessing test.
#
# This test registers a single client with the option 'ignorepid' and spawns a 
# second process that sends PINGs using the same client ID. The purpose of this 
# test is to verify whether a client ID can be used from a different process than 
# the one that registered it. The expectation is that this should work.
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
import multiprocessing

def second_process_func(client_id):
    for _ in range(5):
        time.sleep(1)
        check_cmd(f"PING {client_id}", f"PING {client_id}\n", expect="OK")

    unregister(clientID, expect="OK")

broker = TestBroker()
broker.start()

clientID = register("testclient", ignorepid=True)

# Start secondary process that also sends PINGs
second_process = multiprocessing.Process(target=second_process_func, args=(clientID,))
second_process.start()

second_process.join()
broker.stop()
