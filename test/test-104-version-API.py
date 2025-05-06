#!/usr/bin/env python3

from common import *
import socket
import re

def valid(label, cmd, expect="OK"):
    reply = send_socket_command(cmd, SOCKET_PATH)
    lines = set(line.strip() for line in reply.splitlines())

    expected = {
        f"daemon_version={PACKAGE_VERSION}",
        f"protocol_version={SOCKET_PROT_VERSION}",
    }

    missing = expected - lines
    if expect == "OK":
        if missing:
            fail(f"{label}: unexpected reply: '{reply}'")
        log_step(f"{label}: version command accepted and valid")
    else:
        if not missing:
            fail(f"{label}: expected error, but got: '{reply}'")
        log_step(f"{label}: version command rejected as expected")

def invalid(label, cmd):
    valid(label, cmd, expect="ERROR")

broker = TestBroker()
broker.start()

valid("VERSION",                    f"VERSION\n")
valid("VERSION + tws",              f"VERSION  \n")
valid("VERSION + nl + junk",        f"VERSION\njunk")

invalid("no newline",               f"VERSION")
invalid("VERSION + lws",            f"  VERSION\n")
invalid("VERSION + junk",           f"VERSIONblah\n")
invalid("VERSION + ' ' + junk",     f"VERSION blah\n")
invalid("wrong casing: lowercase",  f"version\n")
invalid("wrong casing: camelCase",  f"Version\n")
invalid("empty string",             f"")
invalid("only newline",             f"\n")

invalid("shell injection nonsense", f"VERSION ; rm -rf /\n")

broker.stop()
