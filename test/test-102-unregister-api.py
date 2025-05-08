#!/usr/bin/env python3
#
# test-102-unregister-api.py – input validation and robustness test for the UNREGISTER command
#
# This test sends a variety of UNREGISTER commands to wd-broker to validate that
# only correctly formatted requests are accepted. Valid cases first register a client
# and then unregister it. Invalid cases reuse a static clientID to test rejection logic.
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

broker = TestBroker()
broker.start()

def valid(label, cmd):
    check_cmd(label, cmd, expect="OK")

def invalid(label, cmd):
    check_cmd(label, cmd, expect="ERROR")

# Valid cases
valid_cases = [
    ("valid + nl",                  "UNREGISTER {}\n"),
    ("valid + ws",                  "UNREGISTER   {}\n"),
    ("valid uppercase id",          "UNREGISTER {}\n"),
    ("valid + trailing ws",         "UNREGISTER {}  \n"),
    ("valid + nl + trailing ws",    "UNREGISTER {}\n  "),
    ("with trailing junk",          "UNREGISTER {}\njunk\n")
]

for label, cmdfmt in valid_cases:
    cid = register("valid-client", timeout_ms=DEFAULT_TIMEOUT*2)
    actual_cmd = cmdfmt.format(cid.upper() if "uppercase" in label else cid)
    valid(label, actual_cmd)

# Unregistering the same clientID again must fail, we reuse the last
# registered clientID from above for this test
invalid("unregister twice", f"UNREGISTER {cid}\n")

# Invalid cases
clientID = register("invalid-client", timeout_ms=DEFAULT_TIMEOUT*2)
bad_ids = derive_invalid_ids(clientID)

for label, test_id in bad_ids.items():
    invalid(f"unregister {label}", f"UNREGISTER {test_id}\n")

invalid("no newline",               f"UNREGISTER {clientID}")
invalid("no id",                    f"UNREGISTER\n")
invalid("extra arg",                f"UNREGISTER {clientID} extra\n")
invalid("with tabs",                f"UNREGISTER\t{clientID}\n")
invalid("with leading space",       f" UNREGISTER {clientID}\n")
invalid("injection semicolon",      f"UNREGISTER {clientID}; rm -rf /\n")
invalid("with junk",                f"UNREGISTER {clientID} junk\n")
invalid("unicode id",               f"UNREGISTER tästID\n")
invalid("control char",             f"UNREGISTER \x01ID123\n")

# Finally, unregister the client as its ID should be still valid
valid("finally valid id",                   f"UNREGISTER {clientID}\n")

broker.stop()
