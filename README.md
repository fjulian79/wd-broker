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

- `--help`:  
  Shows a help message with all available options and exits.

- `--version`:  
  Displays the current version of `wd-broker` and exits.

- `--daemonize`:  
  Runs the broker as a background daemon process. By default, it runs in the foreground.

- `--wd-timeout <seconds>`:  
  Sets the hardware watchdog timeout in seconds. Must be between `10` and `60`. Internally, the broker ticks one second faster than the configured timeout to ensure a safety margin.

- `--socket-path <path>`:  
  Specifies the Unix domain socket path used for communication. Default: `/run/wd-broker.sock`

- `--service-user <username>`:  
  Drops privileges to the specified user after initialization. Must not be `root`. Default: `wd-broker`

- `--syslog-facility <facility>`:  
  Specifies the syslog facility when running as a daemon. Supported values:
  - `LOG_DAEMON` (default)
  - `LOG_USER`
  - `LOG_LOCAL0` through `LOG_LOCAL7`

- `--no-watchdog`:  
  Disables the hardware watchdog logic. The broker runs in simulation/test mode.

---

### Protocol

Clients communicate with the broker via a Unix domain socket (default: `/run/wd-broker.sock`).
The protocol is simple and line-based. Each connection handles exactly **one command**, and is then closed by the broker.

**General Rules:**
- Commands are case-sensitive.
- Maximum command length: 127 characters.
- Commands may be terminated with `\n` (newline). Trailing newlines are ignored.
- Up to 64 clients can be registered concurrently.
- Commands are processed synchronously per connection.

#### REGISTER `<name>` `<timeout_ms>`
Registers a new client.

- `name`: Printable ASCII string, max 63 characters, no spaces.
- `timeout_ms`: Integer between 5000 and 300000 (5s to 5min).
- Response: `OK <clientID>` or `ERROR <reason>`
- `clientID` is a unique 16-character lowercase hex string assigned by the broker.

Example:
```
REGISTER sensorA 15000
→ OK a4b7c8e912d0fc13
```

#### PING `<clientID>`
Renews the client's heartbeat.

- The client's PID must match the PID stored during registration.
- Response: `OK`, `ERROR wrong PID`, `ERROR unknown clientID`, or syntax-related error.

Example:
```
PING a4b7c8e912d0fc13
→ OK
```

#### UNREGISTER `<clientID>`
Removes a client from the active list.

- PID validation is enforced.
- Response: `OK` or `ERROR <reason>`

Example:
```
UNREGISTER a4b7c8e912d0fc13
→ OK
```

#### STATUS
Returns system-level information and all active clients.

- Only allowed if the calling process is root.
- Response includes watchdog timeout, number of registered clients, and a list of known clients:
  `<clientID> <pid> <name> <timeout_ms>`

Example:
```
STATUS
→ Watchdog timeout: 10 seconds
→ Clients registered: 2
→ 4f3c0d9e8a1b4f21 1234 watchdogd 10000
→ 1a2b3c4d5e6f7g89 4321 sensorX 30000
```

---

### Notes and Limitations

- Each client is uniquely identified by both its `clientID` and its process ID (`pid`).
- Heartbeat failures result in the broker logging a critical error and exiting to trigger a system reset.
- The broker does not support authentication; security relies on the Unix socket permissions and PID matching.
- Socket reuse is checked on startup. If a previous instance left a stale socket, it is removed unless a running broker is detected.
- The broker must be started as root if hardware watchdog access is enabled, but will drop privileges before accepting connections.
- Clients must re-register after a broker restart.
- The `LIST` command is diagnostic only and not intended for runtime monitoring by applications.


## Testing

`wd-broker` comes with a set of tests along with a minimalistic test framework build to run them on the embedded build target. To learn more, see [Testing Guide](doc/Tests.md).

## License

This project is licensed under the terms of the **GNU General Public License v3.0**.\
See the [LICENSE](LICENSE) file for details.

