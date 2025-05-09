# Changelog

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
