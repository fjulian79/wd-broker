# Testing `wd-broker`

The repository includes a portable test setup for verifying the broker’s behavior in test mode (`--test`). This mode disables `/dev/watchdog` access and logs events to stdout.

## How to Run Tests

1. Build the broker:
   ```bash
   ./autogen.sh && ./configure && make
   ```

2. Create the test package:
   ```bash
   make test-package
   ```

3. Copy and extract `wd-broker-test.tar.gz` to the target system.

4. Run tests:
   ```bash
   cd wd-broker-test
   ./run-tests.sh
   ```

You can also run individual tests:
```bash
./run-tests.sh test-000-single-client.py
./run-tests.sh test-000-single-client.py test-010-multiple-clients.py
./run-tests.sh test-1*
./run-tests.sh test-020*
```
## Test Descriptions

- `test-client.py`: Registers a client and sends periodic PINGs.
- `test-timeout.py`: Tests timeout behavior when PINGs stop (simulated).
- `common.py`: Shared test logic (register, send_cmd, etc.)
- `run-tests.sh`: Manages test execution, broker startup and shutdown.

## Requirements

- Python 3
- Shell (e.g. bash, busybox ash)
