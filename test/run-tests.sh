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
        if ! kill -0 "$BROKER_PID" 2>/dev/null; then
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
        if kill -0 "$BROKER_PID" 2>/dev/null; then
            kill "$BROKER_PID" 2>/dev/null || true
            wait "$BROKER_PID" 2>/dev/null || true
            echo "[INFO] Broker killed, PID: $BROKER_PID"
        else
            echo "[INFO] Broker process $BROKER_PID already terminated"
        fi
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
        echo "[FAILED] $test_name (broker startup failed)"
        FAILED=$((FAILED + 1))
        return
    fi

    local status=0
    BROKER_LOG="$LOG" python3 "$test_file"
    status=$?

    stop_broker

    if [ $status -eq 0 ]; then
        echo "[PASSED] $test_name"
        PASSED=$((PASSED + 1))
    else
        echo "[FAILED] $test_name (exit code $status)"
        echo "[DEBUG] Broker log:"
        cat "$LOG"
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

