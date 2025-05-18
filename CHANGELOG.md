# Changelog

## [2.0.0-rc2] - 2025-05-18

### Added
- Added logging of WDIOC_GETBOOTSTATUS information when starting wd-broker

## [2.0.0-rc1] - 2025-05-18

### Added
- Support for announced clients ([#1](https://github.com/fjulian79/wd-broker/issues/1))
- Introduced a config file for the broker daemon to define announced clients
- Moved some command line options to the config file
- Added new testcases for announced clients

### Planned for 2.0.0
- Add a option to reject clients with non unique names
- Add a option to reject clients that are not in the config file
- Add logging of WDIOC_GETBOOTSTATUS information when starting wd-broker

## [v1.0.0] - 2025-05-09

### Added
- Initial stable release of the `wd-broker` project
- `wd-broker` daemon to manage watchdog clients via Unix domain sockets
- `wd-ctrl` CLI tool to interact with the broker
- C client library (`libwd-client`) with header file and `libtool`-based build
- Python test suite (`run-tests.sh`, `test-*.py`)
- Support for client timeouts, PID verification, and optional `ignorepid` flag
- Logging to syslog
- Cross-build support

### Known Limitations
- Does not log to tty or /dev/console in case of critical issues.
  This planned but needs mire test.
- Tests could be improved.
