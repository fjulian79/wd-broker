#!/usr/bin/env python3
#
# test-201-config-file.py – validate config file error handling in wd-broker
#
# This test feeds wd-broker with various malformed configuration files
# and checks if the daemon refuses to start as expected.
#
# Copyright 2025 Julian Friedrich
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
import tempfile
import os

def start_with_config(config_path, expected_error_log):
    ok = broker.start(config=config_path, expect="FAIL")
    if not ok:
        fail(f"Broker should have failed with config {config_path}")
    if not broker.check_log(expected_error_log):
        fail(f"Expected error log not found, test based on {config_path} failed")
    log_step(f"{config_path} passed\n")
    broker.clear_log()
    broker.stop()

broker = TestBroker()

start_with_config("cfg/nok-wd-timeout-max.conf",            "Error: Invalid watchdog timeout")
start_with_config("cfg/nok-wd-timeout-min.conf",            "Error: Invalid watchdog timeout")
start_with_config("cfg/nok-client-timeout-min.conf",        "Error: Invalid client timeout")
start_with_config("cfg/nok-client-timeout-max.conf",        "Error: Invalid client timeout")
start_with_config("cfg/nok-unknown-key.conf",               "Error: Unknown config key 'bad_key'")
start_with_config("cfg/nok-bad-value-strict_clients.conf",  "Error: Invalid value for strict_clients")
start_with_config("cfg/nok-bad-value-unique_clients.conf",  "Error: Invalid value for unique_clients")
start_with_config("cfg/nok-duplictated-client.conf",        "Error: Duplicate client name 'beta' in config file")
start_with_config("cfg/nok-missing-assignment.conf",        "Error: Invalid config in line: 6: unique_clients false")
