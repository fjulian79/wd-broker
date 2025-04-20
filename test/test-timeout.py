#!/usr/bin/env python3
from common import register, send_cmd, unregister, fail
import time
import sys

clientID = register("lazyclient", 2000)

print("Sending 1st PING...")
reply = send_cmd(f"PING {clientID}\n", expect="OK")
print("Reply:", reply)

print("Waiting 5 seconds to trigger timeout...")
time.sleep(5)

print("Sending late PING...")
reply = send_cmd(f"PING {clientID}\n")
if not reply.startswith("ERROR"):
    fail(f"Expected timeout error, got: '{reply}'")
print("Reply:", reply)

print("Unregistering...")
unregister(clientID)
sys.exit(0)
