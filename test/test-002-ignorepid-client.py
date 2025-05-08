#!/usr/bin/env python3
#
# test-002-ignorepid-client.py – Basic single-client muliprocessing test.
#
# This test registers a single client with the option 'ignorepid' and spawns a 
# second process that sends PINGs using the same client ID. The purpose of this 
# test is to verify whether a client ID can be used from a different process than 
# the one that registered it. The expectation is that this should work.
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
