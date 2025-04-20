#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BROKER_PATH="$SCRIPT_DIR/wd-broker"
SOCKET="/tmp/wd-broker-test.sock"
LOG="/tmp/wd-broker-test.log"

TOTAL=0
PASSED=0
FAILED=0

start_broker() {
    "$BROKER_PATH" --test > "$LOG" 2>&1 &
    BROKER_PID=$!
    echo "[INFO] Broker started, PID: $BROKER_PID"

    for i in {1..20}; do
        if [ -S "$SOCKET" ]; then
            return 0
        fi
        if ! kill -0 $BROKER_PID 2>/dev/null; then
            echo "[ERROR] Broker process exited unexpectedly!"
            cat "$LOG"
            return 1
        fi
        sleep 0.1
    done

    echo "[ERROR] Socket was not created in time: $SOCKET"
    echo "[DEBUG] Broker log:"
    cat "$LOG"
    return 1
}

stop_broker() {
    if [ -n "$BROKER_PID" ]; then
        kill "$BROKER_PID" 2>/dev/null || true
        wait "$BROKER_PID" 2>/dev/null || true
        echo "[INFO] Broker killed, PID: $BROKER_PID"
        unset BROKER_PID
    fi
}

run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file")

    echo "[TEST] $test_name"
    TOTAL=$((TOTAL + 1))

    start_broker
    if [ $? -ne 0 ]; then
        echo "[FAIL] $test_name (broker startup failed)"
        FAILED=$((FAILED + 1))
        return
    fi

    python3 "$test_file"
    local status=$?

    stop_broker

    if [ $status -eq 0 ]; then
        echo "[ OK ] $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $test_name (exit code $status)"
        echo "[DEBUG] Broker log:"
        cat "$LOG"
        FAILED=$((FAILED + 1))
    fi

    echo
}

if [ $# -eq 1 ]; then
    test_file="$SCRIPT_DIR/$1"
    if [ ! -f "$test_file" ]; then
        echo "[ERROR] Test '$1' not found."
        exit 2
    fi
    run_test "$test_file"
else
    for test_file in "$SCRIPT_DIR"/test-*.py; do
        run_test "$test_file"
    done

    echo "[SUMMARY] $PASSED passed, $FAILED failed, $TOTAL total"
    if [ $FAILED -ne 0 ]; then
        exit 1
    fi
fi
