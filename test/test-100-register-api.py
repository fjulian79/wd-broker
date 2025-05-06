#!/usr/bin/env python3
#
# test-100-register-api.py – input validation and robustness test for the REGISTER command
#
# This test sends a variety of REGISTER commands to wd-broker to validate that
# only correctly formatted requests are accepted. Valid registrations are followed
# by an UNREGISTER. All malformed or malicious inputs must result in an ERROR.
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

from common import *
import re

broker = TestBroker()
broker.start()

default_timeout = DEFAULT_TIMEOUT

def valid(label, cmd):
    reply = check_cmd(label, cmd, expect="OK")
    parts = reply.split()
    if len(parts) != 2:
        fail(f"{label}: unexpected reply format: '{reply}'")
    unregister(parts[1])

def invalid(label, cmd):
    check_cmd(label, cmd, expect="ERROR")

# Valid examples
valid("valid basic",           f"REGISTER client123 {default_timeout}\n")
valid("with ignorepid",        f"REGISTER client123 {default_timeout} ignorepid\n")
valid("valid with dash",       f"REGISTER client-42 {default_timeout}\n")
valid("valid with underscore", f"REGISTER client_abc {default_timeout}\n")
valid("extra whitespace",      f"REGISTER   client123   {default_timeout}\n")
valid("with newline",          f"REGISTER client123 {default_timeout}\n")
valid("with ignorepid + lws",  f"REGISTER client123 {default_timeout}   ignorepid\n")
valid("with ignorepid + tws",  f"REGISTER client123 {default_timeout} ignorepid   \n")


# Invalid syntax
invalid("no newline",          f"REGISTER client123 {default_timeout}")
invalid("lowercase",           f"register client123 {default_timeout}\n")
invalid("SentenenceCase",      f"Register client123 {default_timeout}\n")
invalid("PascalCase",          f"RegIstEr client123 {default_timeout}\n")
invalid("KebabCase",           f"Regi-ster client123 {default_timeout}\n")
invalid("missing timeout",     f"REGISTER client123\n")
invalid("nane with blank",     f"REGISTER client 123 {default_timeout}\n")
invalid("timeout first",       f"REGISTER {default_timeout} client123\n")
invalid("only keyword",        f"REGISTER\n")
invalid("timeout as text",     f"REGISTER client123 three_thousand\n")
invalid("too many args",       f"REGISTER client123 {default_timeout} ignorepid extra\n")
invalid("missing name",        f"REGISTER {default_timeout}\n")
invalid("ignorepid upper",     f"REGISTER client123 {default_timeout} IGNOREPID\n")
invalid("duplicate ignorepid", f"REGISTER client123 {default_timeout} ignorepid ignorepid\n")
invalid("name with tab",       f"REGISTER client\tname {default_timeout}\n")
invalid("name with newline",   f"REGISTER client\nname {default_timeout}\n")

# Edge values
invalid("timeout below min",   f"REGISTER client123 {WD_CLIENT_TIMEOUT_MIN_MS - 1}\n")
invalid("timeout above max",   f"REGISTER client123 {WD_CLIENT_TIMEOUT_MAX_MS + 1}\n")
invalid("timeout zero",        f"REGISTER client123 0\n")
invalid("timeout negative",    f"REGISTER client123 -1\n")
invalid("timeout huge",        f"REGISTER client123 999999999999\n")

# Malformed names
invalid("empty name",          f"REGISTER  3000\n")
invalid("unicode name",        f"REGISTER 👾 3000\n")
invalid("newline injection",   f"REGISTER attacker\nPING other\n")

broker.stop()
