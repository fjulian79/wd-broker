#!/bin/bash
set -e

ARCHIVE_NAME="wd-broker.tar.gz"
TMPDIR=$(mktemp -d)
STAGE="${TMPDIR}/wd-broker"

cleanup() {
    echo "Cleaning up temporary directory..."
    rm -rf "$TMPDIR"
    echo "Done."
}

copy_optional() {
    cp "$@" "$STAGE/tests/" 2>/dev/null || echo "file not found: $*"
}

shopt -s nullglob
trap cleanup EXIT
mkdir -p "$STAGE/tests"

make install DESTDIR="$STAGE" || {
    echo "Build failed. Please check the output above."
    exit 1
}

python3 test/gen_constants.py 
echo "Copying test-tools to tests/..."
copy_optional src/wd-client-test
copy_optional test/constants.py
copy_optional test/common.py
copy_optional test/client-sim.py
copy_optional test/test-*.py
copy_optional test/run-tests.sh

echo "Creating archive $ARCHIVE_NAME..."
tar czf "$ARCHIVE_NAME" -C "$TMPDIR" wd-broker
