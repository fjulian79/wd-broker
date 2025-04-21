#!/usr/bin/env python3
#
# common.py – shared test utilities for wd-broker test suite
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

import socket
import sys
import os

SOCKET_PATH = "/tmp/wd-broker-test.sock"

def log(level, *args):
    valid_levels = {"DEBUG", "INFO", "STEP", "ERROR"}
    if level not in valid_levels:
        raise ValueError(f"Unknown log level: {level}")
    msg = " ".join(str(arg) for arg in args)
    print(f"[{level}] {msg}")

def log_debug(msg):  
    log("DEBUG", msg)

def log_info(msg): 
    log("INFO", msg)

def log_step(msg):   
    log("STEP", msg)

def log_error(msg):  
    log("ERROR", msg)
    
def fail(msg):
    log_error(msg)
    sys.exit(1)

def register(name, timeout_ms, expect="OK"):
    reply = check_cmd(f"REGISTER '{name}'", f"REGISTER {name} {timeout_ms}\n", expect=expect)
    if expect == "OK":
        clientID = reply.split()[1]
        return clientID
    return None

def unregister(clientID, name=None):
    if name:
        label = f"UNREGISTER {name} ({clientID})"
    else:
        label = f"UNREGISTER {clientID}"
    check_cmd(label, f"UNREGISTER {clientID}\n", expect="OK")

def check_cmd(label, cmd, expect=None, timeout_s=30, shall_fail=False):
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.settimeout(timeout_s)
            sock.connect(SOCKET_PATH)
            sock.sendall(cmd.encode())
            reply = sock.recv(128).decode().strip()
            status = reply.split()[0] if reply else ""
            if expect and status != expect:
                fail(f"{label}: expected '{expect}', got: '{status}' (full reply: '{reply}')")

            log_step(f"{label}: got expected response '{reply}'")
            return reply

    except (FileNotFoundError, ConnectionRefusedError, socket.timeout) as e:
        if shall_fail:
            log_step(f"{label}: connection failed as expected ({e})")
            return None
        else:
            fail(f"{label}: connection failed ({e})")