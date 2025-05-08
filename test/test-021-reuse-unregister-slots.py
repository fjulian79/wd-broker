#!/usr/bin/env python3
#
# test-021-reuse-unregister-slots.py – detect resource leaks in repeated UNREGISTER cycles
#
# This test registers and immediately unregisters clients in a loop,
# repeating the cycle WD_MAX_CLIENTS * 2 times to ensure that the broker
# releases internal resources properly. If there is a resource leak or
# improper slot cleanup, the broker will reject registration with ERROR.
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

broker = TestBroker()
broker.start()

for i in range(WD_MAX_CLIENTS * 2):
    label = f"register/unregister cycle {i+1}"
    clientID = register("leaktest", timeout_ms=DEFAULT_TIMEOUT*2)
    check_cmd(label, f"UNREGISTER {clientID}\n", expect="OK")

broker.stop()
