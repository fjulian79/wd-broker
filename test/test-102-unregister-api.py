#!/usr/bin/env python3
#
# test-102-unregister-api.py – input validation and robustness test for the UNREGISTER command
#
# This test sends a variety of UNREGISTER commands to wd-broker to validate that
# only correctly formatted requests are accepted. Valid cases first register a client
# and then unregister it. Invalid cases reuse a static clientID to test rejection logic.
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

# Valid cases
valid_cases = [
    ("valid id",                    "UNREGISTER {}"),
    ("valid id + nl",               "UNREGISTER {}\n"),
    ("valid id again",              "UNREGISTER {}"),
    ("valid + nl",                  "UNREGISTER {}\n"),
    ("valid + ws",                  "UNREGISTER   {}"),
    ("valid uppercase id",          "UNREGISTER {}"),
    ("valid + trailing ws",         "UNREGISTER {}  "),
    ("valid + trailing ws + nl",    "UNREGISTER {}  \n"),
    ("valid + nl + trailing ws",    "UNREGISTER {}\n  "),
]

for label, cmdfmt in valid_cases:
    cid = register("valid-client", timeout_ms=DEFAULT_TIMEOUT*2)
    actual_cmd = cmdfmt.format(cid.upper() if "uppercase" in label else cid)
    valid(label, actual_cmd)

# Unregistering the same clientID again must fail, we reuse the last
# registered clientID from above for this test
invalid("unregister twice", f"UNREGISTER {cid}")

# Invalid cases
clientID = register("invalid-client", timeout_ms=DEFAULT_TIMEOUT*2)
bad_ids = derive_invalid_ids(clientID)

for label, test_id in bad_ids.items():
    invalid(f"unregister {label}", f"UNREGISTER {test_id}\n")

invalid("no id",                    f"UNREGISTER\n")
invalid("extra arg",                f"UNREGISTER {clientID} extra\n")
invalid("with tabs",                f"UNREGISTER\t{clientID}\n")
invalid("with leading space",       f" UNREGISTER {clientID}\n")
invalid("injection semicolon",      f"UNREGISTER {clientID}; rm -rf /\n")
invalid("with newline in id",       f"UNREGISTER {clientID}\nPING {clientID}\n")
invalid("with comment",             f"UNREGISTER {clientID} # comment\n")
invalid("unicode id",               f"UNREGISTER tästID\n")
invalid("control char",             f"UNREGISTER \x01ID123\n")

# Finally, unregister the client as its ID should be still valid
valid("finally valid id",                   f"UNREGISTER {clientID}\n")

broker.stop()
