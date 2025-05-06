#!/usr/bin/env python3
#
# test-021-reuse-unregister-slots.py – detect resource leaks in repeated UNREGISTER cycles
#
# This test registers and immediately unregisters clients in a loop,
# repeating the cycle WD_MAX_CLIENTS * 2 times to ensure that the broker
# releases internal resources properly. If there is a resource leak or
# improper slot cleanup, the broker will reject registration with ERROR.
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

broker = TestBroker()
broker.start()

for i in range(WD_MAX_CLIENTS * 2):
    label = f"register/unregister cycle {i+1}"
    clientID = register("leaktest", timeout_ms=DEFAULT_TIMEOUT*2)
    check_cmd(label, f"UNREGISTER {clientID}\n", expect="OK")

broker.stop()
