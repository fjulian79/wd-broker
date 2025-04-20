#!/usr/bin/env python3
from common import register, check_cmd, fail, log_step, log_info
import time
import os
import sys

log_path = os.environ.get("BROKER_LOG")
clientID = register("lazyclient", 2000)

check_cmd("first PING", f"PING {clientID}\n", expect="OK")

log_step("Waiting 4 seconds to trigger timeout...")
time.sleep(4)

check_cmd("late PING", f"PING {clientID}\n", shall_fail=True)

log_step("Checking broker log for timeout message...")
if not os.path.exists(log_path):
    fail(f"Expected broker log file '{log_path}' not found.")
with open(log_path) as f:
    log = f.read()

if "CLIENT HEARTBEAT TIMEOUT OCCURED" not in log:
    print(log)
    fail("Expected timeout message not found in broker log.")
log_step("Timeout message confirmed in log.")

sys.exit(0)
