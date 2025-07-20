# Changelog

## [2.1.0] - 2025-07-20

### Added
- Added reboot failsafe functionality to the broker daemon and the `wd-ctrl` CLI tool.
  - 'sudo wd-ctrl reboot' activates the reboot failsafe.
  - The reboot timeout can be configured in the config file, default is 60 seconds.
  - If activated, the broker damon will reconfigure the watchdog device to reboot the system after the configured timeout and quit.
  - Purpose of the reboot failsafe is to ensure that the system reboots even if it gets stuck while sutting down.
- Updated the documentation to reflect the new reboot failsafe functionality.

## [2.0.0] - 2025-07-20

### Fixed
 - Improved error output while parsing the config file.
 - Moved 'opened watchdog' log message to the correct place in the code.
 - Fixed log messages when setting socket permissions and ownership.
 - Added testcases 
   - test-023-unique-clients.py
   - test-024-unique-strict-clients.py
   - test-201-config-file.py
 - Improved test framework

## [2.0.0-rc5] - 2025-06-04

### Fixed
- Set watchdog feed interval to wd_timeout / 2 to improve robustness.
- Replaced potentially blocking read() in wd_client_ping with read_with_timeout().
- Fix off-by-one error in configuration parser preveting usage of maximum number of clients.
- Fixed WD_REGISTER_SCANF_FORMAT to expect an unsigned int as timeout value.
- Fixed unintended fall trough while parsing command line options.
- Improved make_clientID() by using random data more efficently.
- Improved trim() function for robustness and empty string handling
- Improved documentation.

## [2.0.0-rc4] - 2025-05-22

### Added
- Added sync() to the watchdog idle loop to persist data before hardware reset.

### Fixed
- Removed deprecated section in the config file template
- Replaced configure.ac hack by proper solution to set sysconfdir in the code.
- Fixed non portable printf format string in `wd-broker.c`
- Improved documentation.

## [2.0.0-rc3] - 2025-05-18

### Added
- Added a config file option to reject clients with non unique names
- Added a config file option to reject clients that are not in the config file
- Added support for listing announced clients via the `wd-ctrl` CLI tool
- Added support for unregistering announced clients via the `wd-ctrl` CLI tool
- Updated the Documentation

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
  This planned but needs more test.
- Tests could be improved.
