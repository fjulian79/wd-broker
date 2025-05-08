#!/bin/bash
#
# run-test.sh – Lightweight runner for Python-based wd-broker test suite.
#
# Executes one or more test scripts matching a pattern or runs all test-*.py
# scripts in the current directory. Collects summary stats and exits with a
# failure code if any test fails.
#
# Includes cleanup logic to kill lingering wd-broker instances in test mode.
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TOTAL=0
PASSED=0
FAILED=0

run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file")

    echo "[TEST] $test_name"
    TOTAL=$((TOTAL + 1))

    local status=0
    python3 "$test_file"
    status=$?

    if [ $status -eq 0 ]; then
        echo "[PASSED] $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "[FAILED] $test_name (exit code $status)"
        FAILED=$((FAILED + 1))
    fi

    echo
}

if [ $# -ge 1 ]; then
    matched=0
    for pattern in "$@"; do
        for test_file in "$SCRIPT_DIR"/$pattern; do
            if [ -f "$test_file" ]; then
                run_test "$test_file"
                matched=1
            fi
        done
    done

    if [ $matched -eq 0 ]; then
        echo "[ERROR] No test files matched: $*"
        exit 2
    fi
else
    for test_file in "$SCRIPT_DIR"/test-*.py; do
        run_test "$test_file"
    done
fi

echo "[SUMMARY] $PASSED passed, $FAILED failed, $TOTAL total"
if [ $FAILED -ne 0 ]; then
    exit 1
fi

existing_pid=$(ps -eo pid,args | grep '[w]d-broker --test' | awk '{ print $1 }')
if [ -n "$existing_pid" ]; then
    echo "[WARN] Found wd-broker process in test mode (PID $existing_pid), sending SIGKILL"
    kill -9 "$existing_pid"
    sleep 0.2
fi
