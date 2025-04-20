#!/usr/bin/env python3
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

def register(name, timeout_ms):
    reply = check_cmd(f"REGISTER {name}", f"REGISTER {name} {timeout_ms}\n", expect="OK")
    clientID = reply.split()[1]
    log_info(f'Registered client "{name}", clientID: {clientID}')
    return clientID

def unregister(clientID):
    check_cmd(f"UNREGISTER {clientID}", f"UNREGISTER {clientID}\n", expect="OK")

def check_cmd(label, cmd, expect=None, shall_fail=False):
    log_step(f"Sending {label}...")
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(SOCKET_PATH)
            sock.sendall(cmd.encode())
            reply = sock.recv(128).decode().strip()

            if shall_fail:
                fail(f"{label}: command unexpectedly succeeded: {reply}")

            if expect and not reply.startswith(expect):
                fail(f"{label}: expected '{expect}', got: '{reply}'")

            log_step(f"{label}: got expected response '{reply}'")
            return reply

    except (FileNotFoundError, ConnectionRefusedError) as e:
        if shall_fail:
            log_step(f"{label}: connection failed as expected ({e})")
            return None
        else:
            fail(f"{label}: connection failed unexpectedly ({e})")
