# wd-broker Configuration File

`wd-broker` can be configured via a configuration file, the default location is: `/${sysconfdir}/wd-broker.conf`
  - `${sysconfdir}` can be controlled via `--sysconfdir` when running `./configure`
  - Set it to a path of your choice via `./configure --sysconfdir=/usr/local`
  - If not defined via `./configure` it defaults to `/etc`

You can also provide a custom configuration file location when starting the daemon. Use the `--config-file <path>` command line option.

## Syntax
- Within the config file, lines starting with `#` are comments, empty lines are ignored.
- Options are defined by name-value pairs in the format `option = value`
- Announced clients are defined in sections starting with `[client <name>]`
  - Within these sections options per client can be defined.
  - The given client name shall not contain ']' or exceed 63 characters. Otherwise the broker will reject the config file.

## Configuration options

### socket_path
`socket_path=<path>`
* Default: `/run/wd-broker.sock`

Defines the path to the Unix domain socket used for client communication.

### service_user
`service_user=<user>`
* Default: `wd-broker`

Service user to drop privileges to after initialization. This user must exist and must NOT be root.

### wd_timeout_s
`wd_timeout_s=<value>`
* Default: `10`
* Minimum: `10`
* Maximum: `60`

Defines the hardware watchdog timeout in seconds. This is the time the watchdog will wait before triggering a system reset if a missing client heartbeat is detected.

### strict_clients
`strict_clients=true|false`
* Default: `false`

When set to 'true', only clients explicitly announced in the configuration file are allowed to register. Use this setting when the list of clients is fixed and known in advance, and strict control is more important than flexibility.

***WARNINGS:***
* The broker daemon will refuse to start if no clients are defined.
* The broker daemon will reject all unregistration requests sent by clients. Clients are forced to stay alive and must send heartbeat messages continuously to keep the watchdog alive, they are not allowed to quit.
* For maintenance purposes, it is still possible to unregister a client using the 'wd-ctrl' command-line tool, but this requires root privileges.

### unique_clients
`unique_clients=true|false`
* Default: `false`

When set to true clients may only register if their name is not already registered by another active client.

***WARNINGS:*** 
* When set to 'true', each announced client must have a unique name. 
* The broker daemon will refuse to start if duplicate names are found in the configuration file. 

### Announced clients
`[client <name>]`

Each client section starts with `[client <name>]`
* The client name must not be longer then 63 characters and may contain any printable character except ']'
* The client name should be unique (recommended), see `unique_clients` option on how to enforce this.

***ATTENTION:*** Announced clients must register with the broker daemon within the timeout period (see below), or the wd-broker daemon will stop feeding the watchdog and the system will reboot. Please read also the 'System design recomendations' section in the [README](README.md) file.

This mechanism ensures that all critical applications are running and responsive after a reboot. If a required client is missing or fails to start, the system will not remain in a potentially unsafe state.

For each client, you can specify the following options:

#### timeout_ms
`timeout_ms=<value>`
* Default: 300000 (5 minutes)
* Minimum: 5000 (1 second)
* Maximum: 300000 (5 minutes)

The time in milliseconds the client has to register with the wd-broker daemon compared to the time the daemon started.

## Default configuration file template
The project comes with a default configuration file ([src/wd-broker.conf](src/wd-broker.conf)) including all available options along with comments explaining their purpose. It is automatically installed at `/${sysconfdir}/wd-broker.conf` via `make install`, but permissions and ownership must be set manually.

## Further Restrictions
* If `wd-broker`can't find the configuration file when started, it will refuse to start and print an error message. This is to prevent false assumptions about the daemon's behavior. It is also prohibited to use a symbolic link as configuration file. 
* Before using the configuration file, its ownership and permissions are checked. The file must be owned by the configured service user (default: `wd-broker`) and must not be writable by others.
