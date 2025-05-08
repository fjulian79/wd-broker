#!/usr/bin/env python3
#
# test-104-version-API.py – Protocol compliance test for wd-broker VERSION command.
#
# Verifies that wd-broker accepts well-formed VERSION commands and rejects invalid
# variants. Ensures protocol version reporting is strict and robust.
#
#     Copyright 2025 Julian Friedrich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is provided on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Source repository: https://github.com/fjulian79/wd-broker

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
