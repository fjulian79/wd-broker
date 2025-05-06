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
- Optional per-client disabling of PID checks via `REGISTER ... ignorepid`

## Security

For details on security issues, reporting vulnerabilities, and responsible disclosure, see the [Security Policy](.github/SECURITY.md).

For a detailed analysis of the system’s trust boundaries and mitigations, refer to the full threat model: [Threat Model (STRIDE)](doc/ThreatModel.md)

## Contributing

Any kind of contribution (issues, pull requests or just feedback) is welcome!  
See [CONTRIBUTING.md](.github/CONTRIBUTING.md) for more information.

## Usage
```bash
$ wd-broker --help
Usage: wd-broker [OPTIONS]

Options:
  --help                    Show this help message and exit
  --version                 Show version information and exit
  --daemonize               Run as a daemon (default: false)
  --wd-timeout <seconds>    Set hardware watchdog timeout (default: 10 seconds)
                            Must be between 10 and 60 seconds
  --socket-path <path>      Set the Unix domain socket path (default: /run/wd-broker.sock)
  --service-user <user>     Set the service user to drop privileges to (default: wd-broker)
                            ATTENTION: Must not be 'root'
  --syslog-facility <name>  Set syslog facility when running as a daemon
                            Supported values: LOG_DAEMON (default), LOG_USER,
                            LOG_LOCAL0 through LOG_LOCAL7
  --no-watchdog             Disable hardware watchdog (test mode, default: false)

Examples:
  wd-broker --wd-timeout 15 --socket-path /run/your-own.sock
  wd-broker --daemonize --service-user youruser
```

You may have to add a dedicated service user and a correspunding group:
```bash
# First create the group
sudo groupadd --system wd-clients

# Then create a user which can't log in and has no home directory,
# but is a member of the group created previously
sudo useradd --system --no-create-home --shell /usr/sbin/nologin \
  --gid wd-clients --groups wd-clients wd-broker

# You may want to add others or yourself to the new group
sudo usermod -aG wd-clients <user>
```

## wd-ctrl: Minimal Command-Line Tool

The `wd-ctrl` tool is a helper utility provided with this project to simplify manual interaction with the broker:

### Usage

```bash
$ wd-ctrl --help
Usage: wd-ctrl [OPTIONS] status | unregister <clientID|name>

Options:
  --help                    Show this help message and exit
  --version                 Show version information and exit
  --socket-path <path>      Use custom socket path (default: /run/wd-broker.sock)

Commands:
  status                    Show the current status of the broker and all clients
  unregister <clientID|name>
                            Unregister a client by its ID or name

Examples:
  wd-ctrl status
  wd-ctrl unregister 4f3c0d9e8a1b4f21
  wd-ctrl --socket-path /run/custom.sock status
```

### Real World Example:

```bash
$ sudo wd-ctrl status
daemon_version    : 0.32.0
protocol_version  : 0.2
wd_timeout_s      : 10
active_clients    : 5

Client ID          PID    Name                 Timeout (ms)  pidCheck
---------------------------------------------------------------------
3e3c89d422bd8e0e   107192 alpha                5000          on      
d3142953f9fbc998   107192 beta                 5000          on      
3d8059154fd9e903   107192 gamma                5000          on      
147a5b1fef290064   107192 delta                5000          off     
08cd8f39bfaeadc4   107192 alpha                5000          on  
```

Force unregistration of a client 
```bash
$ sudo wd-ctrl unregister <clientID | name>
Client 'beta' unregistered successfully.
```
## Protocol

Clients communicate with the broker via a Unix domain socket (default: `/run/wd-broker.sock`).
The protocol is simple and line-based. Each connection handles exactly **one command**, and is then closed by the broker.

**General Rules:**
- Commands are case-sensitive.
- Maximum command length: 127 characters.
- Commands **must be terminated with `\n` (newline)**.
- Trailing newlines are ignored.
- Up to 64 clients can be registered concurrently.
- Commands are processed synchronously per connection.

#### REGISTER `<name>` `<timeout_ms>` `[ignorepid]`
Registers a new client.

- `name`: Printable ASCII string, max 63 characters, no spaces.
- `timeout_ms`: Integer between 5000 and 300000 (5s to 5min).
- Optional third argument `ignorepid` disables PID checking for this client.
- Response: `OK <clientID>` or `ERROR <reason>`
- `clientID` is a unique 16-character lowercase hex string assigned by the broker.

Example:
```
REGISTER sensorA 15000\n
→ OK a4b7c8e912d0fc13

REGISTER testtool 10000 ignorepid\n
→ OK 7c6b7f3e91ab8d24
```

#### PING `<clientID>`
Renews the client's heartbeat.

- The client's PID must match the PID stored during registration unless `ignorepid` was used.
- Response: `OK`, `ERROR wrong PID`, `ERROR unknown clientID`, or syntax-related error.

Example:
```
PING a4b7c8e912d0fc13\n
→ OK
```

#### UNREGISTER `<clientID>`
Removes a client from the active list.

- PID validation is enforced unless `ignorepid` was used.
- Response: `OK` or `ERROR <reason>`

Example:
```
UNREGISTER a4b7c8e912d0fc13\n
→ OK
```

#### STATUS
Returns system-level information and all active clients.

- Only allowed if the calling process is root.
- Response includes multiple lines in the following format:
  - `daemon_version=<version>`
  - `protocol_version=<version>`
  - `wd_timeout_s=<timeout>`
  - `active_clients=<count>`
  - One line per active client: `<clientID> <pid> <name> <timeout_ms> <pidCheck>`

Example:
```
STATUS\n
→ daemon_version=0.28.0
→ protocol_version=0.1
→ wd_timeout_s=10
→ active_clients=2
→ 4f3c0d9e8a1b4f21 1234 watchdogd 10000 0
→ 1a2b3c4d5e6f7g89 4321 sensorX 30000 1
```

#### VERSION
Returns the version information of the daemon and protocol.

- May be called by any client (no special privileges required).
- Response includes two lines in the following format:
  - `daemon_version=<version>`
  - `protocol_version=<version>`
- No additional arguments are allowed.

## Notes and Limitations

- Each client is uniquely identified by both its `clientID` and its process ID (`pid`) unless `ignorepid` was used.
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
