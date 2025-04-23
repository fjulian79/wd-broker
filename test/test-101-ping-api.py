#!/usr/bin/env python3
#
# test-101-ping-api.py – input validation and robustness test for the PING command
#
# This test sends various PING commands to wd-broker to verify correct handling.
# It includes both valid and invalid inputs. A valid client is registered at the
# beginning and unregistered at the end. Invalid inputs are expected to trigger
# ERROR responses.
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

broker = TestBroker()
broker.start()

def valid(label, cmd):
    check_cmd(label, cmd, expect="OK")

def invalid(label, cmd):
    check_cmd(label, cmd, expect="ERROR")

# Setup: register a valid client
clientID = register("pingtest", 10000)

#derive invalid clientIDs
bad_ids = derive_invalid_ids(clientID)
clientID_invalid = bad_ids['invalid id']

# Valid PINGs
valid("valid PING",                 f"PING {clientID}")
valid("valid + nl",                 f"PING {clientID}\n")
valid("valid + ws",                 f"PING   {clientID}")
valid("valid uppercase id",         f"PING {clientID.upper()}")
valid("valid + trailing ws",        f"PING {clientID}  ")
valid("valid + trailing ws + nl",   f"PING {clientID}  \n")
valid("valid + nl + trailing ws",   f"PING {clientID}\n  ")

# Invalid PINGs
for label, test_id in bad_ids.items():
    invalid(f"{label}",             f"PING {test_id}\n")

invalid("valid + leading ws",       f"  PING {clientID}")
invalid("lowercase",                f"ping {clientID}")
invalid("PascalCase",               f"PiNg {clientID}")
invalid("SentenenceCase",           f"Ping {clientID}")
invalid("KebabCase",                f"PI-NG {clientID}")
invalid("missing argument",         f"PING")
invalid("extra whitespace",         f"PING  ")
invalid("extra ws + nl",            f"PING  \n")
invalid("only newline",             f"\n")
invalid("empty",                    f"")
invalid("invalid clientID",         f"PING invalidid")
invalid("too many arguments",       f"PING {clientID} extra")
invalid("too many arguments + nl",  f"PING {clientID}\nextra")
invalid("malformed id chars",       f"PING !@#$%^&*()")
invalid("newline inj. OK NOK",      f"PING {clientID}\nPING {clientID_invalid}")
invalid("newline inj. OK OK",       f"PING {clientID}\nPING {clientID}")
invalid("newline inj. NOK OK",      f"PING {clientID_invalid}\nPING {clientID}")
invalid("newline inj. NOK NOK",     f"PING {clientID_invalid}\nPING {clientID_invalid}")
invalid("multiple inj. OK NOK",     f"PING {clientID} PING {clientID_invalid}")
invalid("multiple inj. OK OK",      f"PING {clientID} PING {clientID}")
invalid("multiple inj. NOK OK",     f"PING {clientID_invalid} PING {clientID}")
invalid("multiple inj. NOK NOK",    f"PING {clientID_invalid} PING {clientID_invalid}")
invalid("PING + tab",               f"PING {clientID}\\t")
invalid("PING + null",              f"PING {clientID}\\x00")

broker.stop()
