# wd-broker

`wd-broker` is a lightweight watchdog supervisor for Linux-based systems, designed to manage hardware watchdogs and ensure system reliability by coordinating heartbeat signals from multiple applications. It acts as the **central authority for feeding the hardware watchdog** (`/dev/watchdog`) and allows multiple heterogeneous applications to register themselves, define individual timeout values, and send heartbeat messages.

Designed for **reliability-critical systems**, `wd-broker` ensures that the hardware watchdog only gets fed when **all registered clients are responsive** – enabling true system resets when necessary.

## Features

* Centralized control over `/dev/watchdog`
* Simple, line-based protocol over Unix domain socket
* Each client defines its own heartbeat timeout
* Announced (mandatory) clients can be defined in the configuration file
* If an announced client does not register within its timeout, system reset will be initiated
* Strict check of config file ownership and permissions
* Must be started as root but runs as a non-root user after initializing `/dev/watchdog` for enhanced security (privilege dropping)
* Restricts access to the used Unix domain socket to authorized users or groups (access control)
* Works with C binaries, Python scripts, shell tools, or whatever can use a Unix domain socket
* Fail-safe: If the broker crashes or a client misses its deadline, the system will reboot
* Optional per-client disabling of PID checks via `REGISTER ... ignorepid`
* Written with portability in mind

## Changelog

See the [CHANGELOG](./CHANGELOG.md) for a full list of changes.

## Security

For details on security issues, reporting vulnerabilities, and responsible disclosure, see the [Security Policy](.github/SECURITY.md).

For a detailed analysis of the system’s trust boundaries and mitigations, refer to the full threat model: [Threat Model (STRIDE)](doc/ThreatModel.md)

## Contributing

Any kind of contribution (issues, pull requests or just feedback) is welcome!
See [CONTRIBUTING.md](.github/CONTRIBUTING.md) for more information.

## Building and Installation
`wd-broker` is a C project and can be built using the standard `./configure`, `make`, and `make install` commands. At time of writing, there are no additional dependencies required to build the project. The project uses the GNU Autotools build system and libtool for shared library support. The project is designed to be portable and should work on most Linux distributions. The included tests are written in Python and require Python 3 to run. 

Define the configuration file location via `--sysconfdir` if you want to change the default location (`/etc`).

Please note that `make install` will not set configuration file permissions or ownership. This must be done manually after installation. This is to avoid errors due to missing users or groups.

To build and install natively, run:
````
./autogen.sh
./configure
make
sudo make install
````

Cross-compilation is supported via the `--host` parameter of `./configure`.
In this case, do not use `--prefix` to set the staging directory, use `make install DESTDIR=<path>` instead.
`````
./autogen.sh
./configure --host=<target> --sysconfdir=/usr/local
make
make install DESTDIR=<path>
`````

## wd-broker 

`wd-broker` is a daemon that manages `/dev/watchdog` and provides a UNIX domain socket as an interface for clients or command-line utilities.
  
### Usage

```bash
$ wd-broker --help
Usage: wd-broker [OPTIONS]

Options:
  --help                    Show this help message and exit
  --version                 Show version information and exit
  --config-file <file>      Load configuration from file (default: /${sysconfdir}/wd-broker.conf)
  --daemonize               Run as a daemon (default: false)
  --service-user <user>     Set the service user to drop privileges to (default: wd-broker)
                            ATTENTION: Must not be 'root'
  --syslog-facility <name>  Set syslog facility when running as a daemon
                            Supported values: LOG_DAEMON (default), LOG_USER,
                            LOG_LOCAL0 through LOG_LOCAL7
  --no-watchdog             Disable hardware watchdog (test mode, default: false)

Examples:
  wd-broker --no-watchdog --config-file /tmp/test-config
  wd-broker --daemonize
```
You may need to add a dedicated service user, the corresponding group and set the permissions for the config file::

```bash
# First create the group
sudo groupadd --system wd-clients

# Then create a user which can't log in and has no home directory,
# but is a member of the group created previously
sudo useradd --system --no-create-home --shell /usr/sbin/nologin \
  --gid wd-clients --groups wd-clients wd-broker

# You may want to add others or yourself to the new group
sudo usermod -aG wd-clients <user>

# Set the permissions for the config file
sudo chown wd-broker:wd-clients /etc/wd-broker.conf
sudo chmod 640 /etc/wd-broker.conf
```

### Configuration file

`wd-broker` can be configured via a configuration file (default location: `/${sysconfdir}/wd-broker.conf`).
  - `${sysconfdir}` can be controlled via `--sysconfdir` when running `./configure`
  - Set it to a path of your choice via `./configure --sysconfdir=/usr/local`
  - If not defined via `./configure` it defaults to `/etc`

You can also provide a custom configuration file location when starting the daemon. Use the `--config-file <path>` command line option.

#### Syntax
- Within the config file, lines starting with `#` are comments, empty lines are ignored.
- Options are defined by name-value pairs in the format `option = value`
- Announced clients are defined in sections starting with `[client <name>]`
  - Within these sections options per client can be defined.
  - The given client name shall not contain ']' or exceed 63 characters. Otherwise the broker will reject the config file.

#### Default configuration file template
The project comes with a default configuration file ([src/wd-broker.conf](src/wd-broker.conf)) including all available options along with comments explaining their purpose. It is automatically installed at `/${sysconfdir}/wd-broker.conf` via `make install`, but permissions and ownership must be set manually.

#### Restrictions
If `wd-broker`can't find the configuration file when started, it will refuse to start and print an error message. This is to prevent false assumptions about the daemon's behavior. It is also prohibited to use a symbolic link as configuration file. Before using the configuration file, its ownership and permissions are checked. The file must be owned by the configured service user (default: `wd-broker`) and must not be writable by others.

### Announced Clients
Announced clients are clients that are predefined in the configuration file under `[client <name>]` sections.
These clients must register with the broker within their specified timeout after the daemon starts.
If an announced client fails to register in time, the broker will stop feeding the hardware watchdog, causing a system reset.

**Why announced clients?**
This mechanism ensures that all critical applications are running and responsive after a reboot. If a required client is missing or fails to start, the system will not remain in a potentially unsafe state.

### Short wd-broker design description

This is a short step-by-step description of what the daemon does:

- Parse parameters and validate them.
- Ensure the service user is not `root`.
- Check ownership and permissions of the config file, if OK, parse it.
- Check if the socket path is ready to be used.
- Create the socket, start listening, and set appropriate permissions.
- Open `/dev/watchdog` as late as possible, ensuring everything else is ready.
- Drop privileges to the specified service user.
- Fork the process and open syslog for logging (until now errors are written to stderr).
- Enter the main loop:
  - Wait in `select()` for client messages, the timer to feed the watchdog, or a timeout.
  - Process client messages if any are received.
  - Check if any registered or announced client has timed out.
  - Feed the watchdog if it is time to do so.
  - Repeat until the process is killed or a client timeout occurs.
- If a client timeout occurs (registered or announced client), enter an infinite loop (`while(1)`) and wait for the system reset.
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
* Structured diagnostics: Use the return value as first indication, write logs just in case of an error. The status message is valid until the next operation.
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
        // This is not just about process liveness – it is about functional correctness.
        if (wd_client_ping(&client) != WD_CLIENT_OK) {
            // This should never happen under normal conditions.
            // If ping fails, wd-broker will stop feeding the hardware watchdog, triggering a reboot.
            printf("wd_client_ping failed: %s\n", client.status);
            break;
        }

        // Sleep for 5 seconds.
        // Choose this delay carefully – your ping interval must always stay within the timeout window.
        // Consider jitter, load spikes, or scheduler delays on the system.
        // Note that wd-broker or libwd-client do not add margins as every use case has its own needs.
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

The protocol is simple and line-based, using a Unix domain socket for communication. The broker listens on a socket, and clients can connect to it to send commands. See [Protocol Documentation](doc/Protocol.md) for a detailed description of the protocol.

## Notes and Limitations

* Each client is uniquely identified by both its `clientID` and its process ID (`pid`) unless `ignorepid` was used.
* Heartbeat failures result in the broker logging a critical error and exiting to trigger a system reset.
* The broker does not support authentication; security relies on the Unix socket permissions and PID matching.
* Socket reuse is checked on startup. If a previous instance left a stale socket, it is removed unless a running broker is detected.
* The broker must be started as root if hardware watchdog access is enabled, but will drop privileges before accepting connections.
* Clients must re-register after a broker restart.
* The `LIST` command is diagnostic only and not intended for runtime monitoring by applications.

## Testing

`wd-broker` comes with a set of tests along with a minimalistic test framework. To learn more, see [Testing Guide](doc/Tests.md).

## License

This project is licensed under the terms of the **Apache License 2.0**.
See the [LICENSE](LICENSE) file for details.
