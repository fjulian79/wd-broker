# wd-broker

> ⚠️ **Work in Progress**: This project is under active development and not yet production-ready. Use at your own risk. Interfaces and behavior may change at any time.

`wd-broker` is a lightweight, POSIX-compliant watchdog supervisor for Linux-based systems. It acts as the **central authority for feeding the hardware watchdog** (`/dev/watchdog`) and allows multiple heterogeneous applications to register themselves, define individual timeout values, and send heartbeat messages.

Designed for **reliability-critical systems**, `wd-broker` ensures that the hardware watchdog only gets fed when **all registered clients are responsive** – enabling true system resets when necessary.


## Features

- Centralized control over `/dev/watchdog`
- Simple, line-based protocol over Unix domain socket
- Each client defines its own timeout
- Works with C binaries, Python scripts, shell tools, or inside containers
- Test mode for development and unit testing (no real watchdog triggered)
- Fail-safe: If the broker crashes or a client misses its deadline, the system will reboot
- Clean and efficient implementation in portable C (POSIX)
- Uses `timerfd` for precise and decoupled watchdog ticking


## Threat Model

[![Security: STRIDE Threat Model](https://img.shields.io/badge/security-STRIDE-blue)](doc/ThreatModel.md)

For a detailed analysis of the system’s trust boundaries and mitigations, refer to the full threat model: [Threat Model (STRIDE)](doc/ThreatModel.md)

## Usage

### Start the broker

```bash
# Run in production mode (will open /dev/watchdog)
./wd-broker

# Run in test mode (no access to /dev/watchdog, useful for development)
./wd-broker --test
```

### Command-line options

- `--test`: run without accessing `/dev/watchdog`
- `--interval <ms>`: set the watchdog tick interval (default: 1000 ms)
- `--version`: show version
- `--help`: show usage info


## Testing

`wd-broker` comes with a set of tests along with a minimalistic test framework build to run them on the embedded build target. To learn more, see [Testing Guide](doc/Tests.md).

## Protocol

Clients talk to the broker via a Unix domain socket (`/tmp/wd-broker.sock`).\
The protocol is simple and line-based:

- `REGISTER <name> <timeout_ms>` → responds with `OK <uid>` or `ERROR`
- `PING <uid>` → responds with `OK` or `ERROR`
- `UNREGISTER <uid>` → responds with `OK` or `ERROR`

---

## License

This project is licensed under the terms of the **GNU General Public License v3.0**.\
See the [LICENSE](LICENSE) file for details.

---

## Repository

[https://github.com/fjulian79/wd-broker](https://github.com/fjulian79/wd-broker)

Issues, pull requests and feedback are welcome!
