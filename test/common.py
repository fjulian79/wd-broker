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

import subprocess
import atexit
import time
import socket
import sys
import os
import tempfile

MAX_CLIENTS = 64
DEFAULT_TIMEOUT = 5000
SOCKET_PATH = "/tmp/wd-broker-test.sock"

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
TEST_BINARY = os.path.join(SCRIPT_DIR, "wd-broker")

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

class TestBroker:
    """
    Manages the lifecycle of a wd-broker instance in test mode.
    - Starts the broker with stdout/stderr redirected to a temporary log file
    - Clears the log before each start
    - Provides access to log content
    - Automatically stops the broker and deletes the log file
    """

    def __init__(self):
        self.proc = None
        self.started = False
        self.log_file = tempfile.NamedTemporaryFile(prefix="broker-log-", suffix=".log", delete=False)
        self.log_path = self.log_file.name
        self.log_file.close()

    def start(self, expect="OK"):
        if self.started:
            raise RuntimeError("Broker already started")

        # Truncate log before each run
        with open(self.log_path, "w"):
            pass

        self.log_stream = open(self.log_path, "w")
        self.proc = subprocess.Popen(
            [TEST_BINARY, "--test"],
            stdout=self.log_stream,
            stderr=subprocess.STDOUT
        )
        self.started = True
        atexit.register(self.stop)

        try:
            if expect == "OK":
                ret = self.proc.wait(timeout=0.1)
                log_error(f"Broker process failed with code {ret}")
                self.started = False
                self.print_log()
                return False
            else:
                ret = self.proc.wait(timeout=1.0)
                if ret == 0:
                    log_error(f"Broker started, PID: {self.proc.pid}")
                    self.stop()
                    self.print_log()
                    return False
                else:
                    log_step(f"Broker process failed with code {ret}")
                    self.started = False
                    return True

        except subprocess.TimeoutExpired:
            if expect == "OK":
                log_info(f"Broker started, PID: {self.proc.pid}")
                return True
            else:
                log_error("Broker did not exit in time, but failure was expected")
                self.stop()
                self.print_log()
                return False

    def stop(self):
        if not self.started:
            return

        if self.proc:
            if self.proc.poll() is None:
                try:
                    self.proc.terminate()
                    self.proc.wait(timeout=1.0)
                    log_info(f"Broker killed, PID: {self.proc.pid}")
                except Exception:
                    self.proc.kill()
                    log_error(f"Broker force-killed, PID: {self.proc.pid}")
            else:
                log_info(f"Broker process {self.proc.pid} already terminated")
        if self.log_stream and not self.log_stream.closed:
            self.log_stream.close()
        self.started = False

    def is_running(self):
        return self.proc is not None and self.proc.poll() is None

    def get_log(self):
        if not os.path.exists(self.log_path):
            raise FileNotFoundError(f"Broker log file not found: {self.log_path}")
        with open(self.log_path, "r") as f:
            return f.read()

    def print_log(self):
        for line in self.get_log():
            print("[BROKER]", line)

    def __del__(self):
        try:
            if self.log_stream and not self.log_stream.closed:
                self.log_stream.close()
            if os.path.exists(self.log_path):
                os.unlink(self.log_path)
        except Exception:
            pass  # silent cleanup on object destruction

def fail(msg):
    log_error(msg)
    sys.exit(1)

def derive_invalid_ids(valid_id):
    id_len = len(valid_id)
    return {
        "short id":      valid_id[:-1],
        "long id":       valid_id + "ff",
        "nonhex id":     "z" * id_len,
        "blank id":      " " * id_len,
        "mixed id":      valid_id[:-1] + "g",
        "invalid id":    f"{int(valid_id, 16) ^ 0x1:0{id_len}x}",
    }

def register(name, timeout_ms=DEFAULT_TIMEOUT, expect="OK"):
    reply = check_cmd(f"REGISTER '{name}'", f"REGISTER {name} {timeout_ms}\n", expect=expect)
    if expect == "OK":
        clientID = reply.split()[1]
        return clientID
    return None

def unregister(clientID, name=None, expect="OK"):
    if name:
        label = f"UNREGISTER {name} ({clientID})"
    else:
        label = f"UNREGISTER {clientID}"
    check_cmd(label, f"UNREGISTER {clientID}\n", expect=expect)

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
            return e