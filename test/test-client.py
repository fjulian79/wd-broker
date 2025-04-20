#!/usr/bin/env python3
from common import register, send_cmd, unregister
import time
import sys

clientID = register("testclient", 3000)

for i in range(5):
    time.sleep(1)
    reply = send_cmd(f"PING {clientID}\n", expect="OK")
    print("Reply:", reply)

unregister(clientID)
sys.exit(0)
