# wd-broker

> ⚠️ **Work in Progress**: This project is under active development and not yet production-ready. Use at your own risk. Interfaces and behavior may change at any time.

`wd-broker` is a lightweight watchdog supervisor for Linux-based systems, designed to manage hardware watchdogs and ensure system reliability by coordinating heartbeat signals from multiple applications. It acts as the **central authority for feeding the hardware watchdog** (`/dev/watchdog`) and allows multiple heterogeneous applications to register themselves, define individual timeout values, and send heartbeat messages.

Designed for **reliability-critical systems**, `wd-broker` ensures that the hardware watchdog only gets fed when **all registered clients are responsive** – enabling true system resets when necessary.

## Features

* Centralized control over `/dev/watchdog`
* Simple, line-based protocol over Unix domain socket
* Each client defines its own timeout
* Must be started as root but runs as a non-root user after initializing `/dev/watchdog` for enhanced security (privilege dropping)
* Restricts access to the used Unix domain socket to authorized users or groups (access control)
* Works with C binaries, Python scripts, shell tools, or whatever can use a Unix domain socket
* Fail-safe: If the broker crashes or a client misses its deadline, the system will reboot
* Optional per-client disabling of PID checks via `REGISTER ... ignorepid`

## Security

For details on security issues, reporting vulnerabilities, and responsible disclosure, see the [Security Policy](.github/SECURITY.md).

For a detailed analysis of the system’s trust boundaries and mitigations, refer to the full threat model: [Threat Model (STRIDE)](doc/ThreatModel.md)

## Contributing

Any kind of contribution (issues, pull requests or just feedback) is welcome!
See [CONTRIBUTING.md](.github/CONTRIBUTING.md) for more information.

## wd-broker 

`wd-broker` is a daemon that manages `/dev/watchdog` and provides a UNIX domain socket as an interface for clients or command-line utilities.
  
### Usage

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
You may need to add a dedicated service user and the corresponding group:

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

### Short Design Description

This is a short step-by-step description of what the daemon does:

- Parse parameters and validate them.
- Ensure the service user is not `root`.
- Check if the socket path is ready to be used.
- Create the socket, start listening, and set appropriate permissions.
- Open `/dev/watchdog` as late as possible, ensuring everything else is ready.
- Drop privileges to the specified service user.
- Fork the process and open syslog for logging.
- Enter the main loop:
  - Wait in `select()` for client messages, the timer to feed the watchdog, or a timeout.
  - Process client messages if any are received.
  - Check if any registered client has timed out.
  - Feed the watchdog if it is time to do so.
  - Repeat until the process is killed or a client timeout occurs.
- If a client timeout occurs, enter an infinite loop (`while(1)`) and wait for the system reset.
- If no client timeout occurs, attempt to stop the watchdog (if supported by the system).
- Close the socket and exit.

Currently, the main loop uses a one-second timeout for `select()`. This may be reworked in the future to provide users with more control over the interval. However, from my perspective, heartbeating and watchdog feeding should occur at reasonable intervals (e.g., seconds) rather than in milliseconds.

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
$ sudo wd-ctrl unregister beta
Client 'beta' unregistered successfully.
```

## wd-client: Embedded C Library

The `wd-client` library provides a simple C interface to register watchdog clients, send heartbeat pings, and unregister gracefully via the same Unix domain socket protocol used by `wd-ctrl`.

### Overview

* Designed for use in Linux applications
* Implements the same protocol as described below
* Thread-safe (no internal state shared between instances)

### Logging Philosophy

The wd-client library does not perform any internal logging via syslog() or stderr output. Instead, it provides a status buffer in wd_client_t (.status) which is updated with a human-readable error description for each operation.

This approach follows a “caller is in control” philosophy:
* Minimal side effects: No implicit logging or output, making the library easy to handle.
* Structured diagnostics: Use the return value as first indication, write logs just in case of a error. The status message is valid until the next operation.
* Flexible integration: Callers may decide how and when to forward error messages (e.g. printf(), syslog(), journal, log file, etc.).

### Basic Usage

A simple example to illustrate the usage:
```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <wd-client.h>

// Define a client with a default socket path and name.
// This macro helps you set all required fields in one go.
#define WD_EXAMPLE_CLIENT_INIT                          \
    {                                                   \
        .socket_path = SOCKET_PATH_DEFAULT,             \
        .name = "example-client",                       \
        .clientID = {0},                                \
        .status = {0}                                   \
    }

static volatile sig_atomic_t stay_alive = true;
void handle_sigint(int sig) {
    (void)sig;
    stay_alive = false;
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    wd_client_t client = WD_EXAMPLE_CLIENT_INIT;

    // Register the client with a 10-second timeout (10000 ms).
    // All pings must arrive before this interval passes.
    if (wd_client_register(&client, 10000, false) != WD_CLIENT_OK) {
        printf("wd_client_register failed: %s\n", client.status);
        return EXIT_FAILURE;
    }

    // This flag models your application logic.
    // Only send heartbeats if your core logic is still functioning correctly.
    bool functional = true;

    while (functional && stay_alive) {
        // Feed the broker with a heartbeat as long as your application is doing its job.
        // This is not just about process liveness – it's about functional correctness.
        if (wd_client_ping(&client) != WD_CLIENT_OK) {
            // This should never happen under normal conditions.
            // If ping fails, wd-broker will stop feeding the hardware watchdog, triggering a reboot.
            printf("wd_client_ping failed: %s\n", client.status);
            break;
        }

        // Sleep for 5 seconds.
        // Choose this delay carefully – your ping interval must always stay within the timeout window.
        // Consider jitter, load spikes, or scheduler delays on the system.
        // Note that wd-broker or libwd-client do not add margins as every use case has it's own needs.
        usleep(5000 * 1000);
    }

    // On clean exit, unregister the client to avoid unintended watchdog resets.
    if (functional) {
        wd_client_unregister(&client);
    }

    return 0;
}

```
See [include/wd-client.h](include/wd-client.h) for full API documentation.

## Protocol

Clients communicate with the broker via a Unix domain socket (default: `/run/wd-broker.sock`).
The protocol is simple and line-based. Each connection handles exactly **one command**, and is then closed by the broker.

**General Rules:**

* Commands are case-sensitive.
* Maximum command length: 127 characters.
* Commands **must be terminated with `\n` (newline)**.
* Trailing newlines are ignored.
* Up to 64 clients can be registered concurrently.
* Commands are processed synchronously per connection.

#### REGISTER `<name>` `<timeout_ms>` `[ignorepid]`

Registers a new client.

* `name`: Printable ASCII string, max 63 characters, no spaces.
* `timeout_ms`: Integer between 5000 and 300000 (5s to 5min).
* Optional third argument `ignorepid` disables PID checking for this client.
* Response: `OK <clientID>` or `ERROR <reason>`
* `clientID` is a unique 16-character lowercase hex string assigned by the broker.

Example:

```
REGISTER sensorA 15000\n
→ OK a4b7c8e912d0fc13

REGISTER testtool 10000 ignorepid\n
→ OK 7c6b7f3e91ab8d24
```

#### PING `<clientID>`

Renews the client's heartbeat.

* The client's PID must match the PID stored during registration unless `ignorepid` was used.
* Response: `OK`, `ERROR wrong PID`, `ERROR unknown clientID`, or syntax-related error.

Example:

```
PING a4b7c8e912d0fc13\n
→ OK
```

#### UNREGISTER `<clientID>`

Removes a client from the active list.

* PID validation is enforced unless `ignorepid` was used.
* Response: `OK` or `ERROR <reason>`

Example:

```
UNREGISTER a4b7c8e912d0fc13\n
→ OK
```

#### STATUS

Returns system-level information and all active clients.

* Only allowed if the calling process is root.
* Response includes multiple lines in the following format:

  * `daemon_version=<version>`
  * `protocol_version=<version>`
  * `wd_timeout_s=<timeout>`
  * `active_clients=<count>`
  * One line per active client: `<clientID> <pid> <name> <timeout_ms> <pidCheck>`

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

* May be called by any client (no special privileges required).
* Response includes two lines in the following format:

  * `daemon_version=<version>`
  * `protocol_version=<version>`
* No additional arguments are allowed.

## Notes and Limitations

* Each client is uniquely identified by both its `clientID` and its process ID (`pid`) unless `ignorepid` was used.
* Heartbeat failures result in the broker logging a critical error and exiting to trigger a system reset.
* The broker does not support authentication; security relies on the Unix socket permissions and PID matching.
* Socket reuse is checked on startup. If a previous instance left a stale socket, it is removed unless a running broker is detected.
* The broker must be started as root if hardware watchdog access is enabled, but will drop privileges before accepting connections.
* Clients must re-register after a broker restart.
* The `LIST` command is diagnostic only and not intended for runtime monitoring by applications.

## Testing

`wd-broker` comes with a set of tests along with a minimalistic test framework build to run them on the embedded build target. To learn more, see [Testing Guide](doc/Tests.md).

## License

This project is licensed under the terms of the **Apache License 2.0**.
See the [LICENSE](LICENSE) file for details.
