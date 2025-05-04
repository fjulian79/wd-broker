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

from constants import *
import atexit
import os
import pty
import socket
import subprocess
import sys
import tempfile
import threading
import time

SOCKET_PATH = "/tmp/wd-broker-test.sock"
DEFAULT_TIMEOUT = CLIENT_TIMEOUT_MIN_MS * 2
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
        self.log_lines = []
        self._log_thread = None
        self._log_lock = threading.Lock()

    def start(self, expect="OK"):
        if self.started:
            raise RuntimeError("Broker already started")

        print("Starting broker with PTY...")
        master_fd, slave_fd = pty.openpty()

        self.proc = subprocess.Popen(
            [TEST_BINARY, "--no-watchdog", "--socket-path", SOCKET_PATH],
            stdin=subprocess.DEVNULL,
            stdout=slave_fd,
            stderr=subprocess.STDOUT,
            text=True,
            close_fds=True
        )
        os.close(slave_fd)  # close the slave FD in the parent process

        self._log_thread = threading.Thread(target=self._log_reader_from_fd, args=(master_fd,), daemon=True)
        self._log_thread.start()

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

        if self._log_thread:
            self._log_thread.join(timeout=1.0)

        self.started = False

    def is_running(self):
        return self.proc is not None and self.proc.poll() is None

    def get_log(self):
        with self._log_lock:
            return self.log_lines

    def print_log(self):
        with self._log_lock:
            for line in self.log_lines:
                print("[BROKER]", line)

    def _log_reader_from_fd(self, fd):
        try:
            with os.fdopen(fd) as f:
                for line in f:
                    with self._log_lock:
                        self.log_lines.append(line.rstrip())
        except Exception as e:
            with self._log_lock:
                self.log_lines.append(f"[error reading log from PTY: {e}]")

    def __del__(self):
        try:
            self.stop()
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

def register(name, timeout_ms=DEFAULT_TIMEOUT, expect="OK", ignorepid=False):
    cmd = f"REGISTER {name} {timeout_ms}"
    if ignorepid:
        cmd += " ignorepid"
    reply = check_cmd(f"REGISTER '{name}'", cmd + "\n", expect=expect)
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

def send_socket_command(cmd, socket_path, timeout_s=30):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout_s)
        sock.connect(socket_path)
        sock.sendall(cmd.encode())
        return sock.recv(1024).decode().strip()

def check_cmd(label, cmd, expect=None, timeout_s=30, shall_fail=False):
    try:
        reply = send_socket_command(cmd, SOCKET_PATH, timeout_s)
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
