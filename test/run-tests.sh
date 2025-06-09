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
CONFIG_USER="wd-broker"
CONFIG_GROUP="wd-clients"

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

bad_files=()

for cfg in "$SCRIPT_DIR"/cfg/*.conf; do
    [ -e "$cfg" ] || continue
    owner=$(stat -c '%U' "$cfg")
    perms=$(stat -c '%a' "$cfg")
    fix_needed=0

    if [ "$owner" != "$CONFIG_USER" ]; then
        echo "[WARN] Config file $cfg is not owned by $CONFIG_USER (owner: $owner)"
        fix_needed=1
    fi
    if [ $(( (10#${perms:1:1}) & 2 )) -ne 0 ] || [ $(( (10#${perms:2:1}) & 2 )) -ne 0 ]; then
        echo "[WARN] Config file $cfg is writable by group or others (permissions: $perms)"
        fix_needed=1
    fi

    if [ $fix_needed -eq 1 ]; then
        bad_files+=("$cfg")
    fi
done

if [ ${#bad_files[@]} -gt 0 ]; then
    echo "[WARN] The following config files have wrong ownership or permissions:"
    for f in "${bad_files[@]}"; do
        echo "  $f"
    done
    read -p "Fix ownership and permissions for ALL these files? [y/n] " answer
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        for f in "${bad_files[@]}"; do
            sudo chown "$CONFIG_USER:$CONFIG_GROUP" "$f"
            sudo chmod 644 "$f"
            echo "[INFO] Fixed $f"
        done
    else
        echo "[ERROR] Please fix the files manually before running tests."
        exit 3
    fi
fi

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
