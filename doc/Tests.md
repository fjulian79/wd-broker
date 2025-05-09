# Testing `wd-broker`

The repository includes a portable test setup for verifying the broker’s behavior in test mode (`--test`). This mode disables `/dev/watchdog` access and logs events to stdout.

## How to Run Tests

1. Build the broker:
   ```bash
   ./autogen.sh && ./configure && make && make check
   ```

2. Create the test package:
   ```bash
   ./create-package.sh
   ```

3. Copy and extract `wd-broker.tar.gz` to the target system.

4. Run tests:
   ```bash
   tar -axf wd-broker.tar.gz
   cd wd-broker/tests
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

See file headers for details.

## Requirements

- Python 3
- Shell (e.g. bash, busybox ash)
