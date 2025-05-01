# wd-broker

> ⚠️ **Work in Progress**: This project is under active development and not yet production-ready. Use at your own risk. Interfaces and behavior may change at any time.

`wd-broker` is a lightweight watchdog supervisor for Linux-based systems, designed to manage hardware watchdogs and ensure system reliability by coordinating heartbeat signals from multiple applications. It acts as the **central authority for feeding the hardware watchdog** (`/dev/watchdog`) and allows multiple heterogeneous applications to register themselves, define individual timeout values, and send heartbeat messages.

Designed for **reliability-critical systems**, `wd-broker` ensures that the hardware watchdog only gets fed when **all registered clients are responsive** – enabling true system resets when necessary.

## Features

- Centralized control over `/dev/watchdog`
- Simple, line-based protocol over Unix domain socket
- Each client defines its own timeout
- Must be started as root but runs as a non-root user after initializing `/dev/watchdog` for enhanced security (privilege dropping)
- Restricts access to the used Unix domain socket to authorized users or groups (access control)
- Works with C binaries, Python scripts, shell tools, or whatever can use a Unix domain socket
- Fail-safe: If the broker crashes or a client misses its deadline, the system will reboot

## Security

For details on security issues, reporting vulnerabilities, and responsible disclosure, see the [Security Policy](.github/SECURITY.md).

For a detailed analysis of the system’s trust boundaries and mitigations, refer to the full threat model: [Threat Model (STRIDE)](doc/ThreatModel.md)

## Contributing

Any kind of contribution (issues, pull requests or just feedback) is welcome!  
See [CONTRIBUTING.md](.github/CONTRIBUTING.md) for more information.

## Usage

### Start the broker

```bash
# Run in production mode (will open /dev/watchdog)
./wd-broker

# Run in test mode (no access to /dev/watchdog, useful for development)
./wd-broker --test
```

### Command-line options

- `--test`: Run without accessing `/dev/watchdog` (useful for development and testing).
- `--interval <ms>`: Set the watchdog tick interval in milliseconds (default: 1000 ms).
- `--syslog-facility <facility>`: Set the syslog facility for logging (default: `LOG_DAEMON`). Supported values:
  - `LOG_DAEMON`
  - `LOG_USER`
  - `LOG_LOCAL0` through `LOG_LOCAL7`
- `--version`: Display the current version of `wd-broker`.
- `--help`: Show usage information and exit.

## Protocol

Clients talk to the broker via a Unix domain socket (`/tmp/wd-broker.sock`).\
The protocol is simple and line-based:

- `REGISTER <name> <timeout_ms>` → responds with `OK <uid>` or `ERROR`
- `PING <uid>` → responds with `OK` or `ERROR`
- `UNREGISTER <uid>` → responds with `OK` or `ERROR`

Commands may optionally be terminated with a newline (\\n). Newlines are ignored during parsing. This allows compatibility with interactive tools like telnet, socat, or echo.

## Testing

`wd-broker` comes with a set of tests along with a minimalistic test framework build to run them on the embedded build target. To learn more, see [Testing Guide](doc/Tests.md).

## License

This project is licensed under the terms of the **GNU General Public License v3.0**.\
See the [LICENSE](LICENSE) file for details.

