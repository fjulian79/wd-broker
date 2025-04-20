#!/usr/bin/env python3
from common import register, unregister, check_cmd
import time
import sys

clientID = register("testclient", 3000)

for i in range(5):
    time.sleep(1)
    check_cmd(f"PING {clientID}", f"PING {clientID}\n", expect="OK")

unregister(clientID)
sys.exit(0)
