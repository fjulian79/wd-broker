# wd-broker communication protocol

Clients communicate with the broker via a Unix domain socket (default: `/run/wd-broker.sock`).
The protocol is simple and line-based. Each connection handles exactly **one command**, and is then closed by the broker.

**General Rules:**

* Commands are case-sensitive.
* Maximum command length: 127 characters.
* Commands **must be terminated with `\n` (newline)**.
* Trailing newlines are ignored.
* Up to 64 clients can be registered concurrently.
* Commands are processed synchronously per connection.

## REGISTER `<name>` `<timeout_ms>` `[ignorepid]`

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

## PING `<clientID>`

Renews the client's heartbeat.

* The client's PID must match the PID stored during registration unless `ignorepid` was used.
* Response: `OK`, `ERROR wrong PID`, `ERROR unknown clientID`, or syntax-related error.

Example:

```
PING a4b7c8e912d0fc13\n
→ OK
```

## UNREGISTER `<clientID>`

Removes a client from the active list.

* PID validation is enforced unless `ignorepid` was used.
* Response: `OK` or `ERROR <reason>`

Example:

```
UNREGISTER a4b7c8e912d0fc13\n
→ OK
```

## STATUS

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

## VERSION

Returns the version information of the daemon and protocol.

* May be called by any client (no special privileges required).
* Response includes two lines in the following format:

  * `daemon_version=<version>`
  * `protocol_version=<version>`
* No additional arguments are allowed.
