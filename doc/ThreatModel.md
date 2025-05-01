# Threat Model for wd-broker

This document describes the threat model for the `wd-broker` daemon using the STRIDE methodology. The goal is to identify potential security risks and clarify the design decisions that mitigate or acknowledge those risks. This analysis assumes `wd-broker` may be deployed in safety-relevant environments where false positives (accidental reboots) and false negatives (system hangs not triggering a reboot) both carry critical weight.

For details on the STRIDE methodology, see [Wikipedia – STRIDE model](https://en.wikipedia.org/wiki/STRIDE_model).

## System Overview

`wd-broker` is a user-space daemon responsible for supervising access to the hardware watchdog on embedded Linux systems. It listens on a Unix Domain Socket (UDS) for client applications that must:

- Register themselves (with a name and timeout)
- Periodically ping the broker to prove they are alive
- Deregister when shutting down cleanly

If any active client fails to ping within its declared timeout, the broker will stop feeding the hardware watchdog, resulting in a system reboot.

## Assumptions

- Only users in a dedicated group (`wdclients`) and root can access the Unix socket.
- It is acceptable for `root` to be able to unregister any client, assuming that root already has full system control. This is relevant during development and test scenarios, and acceptable in production as well.
- Clients are responsible for managing their registration state securely.
- The system assumes the integrity of the operating system and the `wdclients` group. If a member of the `wdclients` group is compromised, the broker cannot prevent misuse.
- The system should allow reboots in case of actual application failures (e.g., deadlocks), but resist intentional misuse.
- The broker is started as `root` but drops privileges after initialization (see below).

## Privilege Management

`wd-broker` requires elevated privileges only during startup:

- To open `/dev/watchdog`, which typically requires root
- To create the Unix socket with the correct permissions and group ownership

After completing these steps, the broker **drops privileges** using `setgid()` and `setuid()` to run as an unprivileged user within the `wdclients` group. This ensures that even if the broker process is compromised during runtime, the system impact is limited.

To avoid disrupting a running instance, the broker performs a safety check before binding to the socket path. If the socket already exists, it attempts to connect to it. If a process is actively listening, the broker aborts with a clear error message. If the connection fails (e.g., with ECONNREFUSED), the socket is considered stale, and the broker may safely remove and recreate it.

This approach reduces the attack surface and aligns with least-privilege principles.

---

## STRIDE Analysis

### 1. Spoofing (Identity Forgery)

**Threat:** A malicious process attempts to impersonate a legitimate client by using its `clientID`.

**Mitigations:**

- Each client is identified by its wd-broker `clientID` and `pid` during registration.
- The broker verifies the `pid` of the process sending commands (e.g., `PING`, `UNREGISTER`) to ensure it matches the PID of the process that originally registered the `clientID`.
- Only processes belonging to the trusted group `wdclients` can connect to the socket (enforced by Unix file permissions).
- The broker runs with dropped privileges after initialization to reduce spoofing risks if exploited.
- Since the broker cannot enforce secure handling of client state on the client side, a compromised client can continue to interact with the broker using its own `clientID`. This limitation is accepted by design: all members of the `wdclients` group are considered trusted, and authentication serves as a lightweight safeguard against accidental misuse or implementation errors.

---

### 2. Tampering (Data Manipulation)

**Threat:** Malicious modification of data in transit or client state.

**Risks:**

- Modifying ping payloads to falsify liveness
- Corrupting internal state to keep dead clients alive

**Mitigations:**

- The broker verifies the `pid` of the process sending `PING` commands to ensure it matches the original registering process.
- The socket protocol is request-response per connection with stateless handling.
- Broker memory (client table) is private to the process; no external tampering is possible.
- Privilege drop ensures internal state cannot be overwritten by unprivileged processes.

---

### 3. Repudiation (Denying Responsibility)

**Threat:** A client claims it did not perform a certain action (e.g., deregistering).

**Risks:**

- False claims of broker misbehavior

**Mitigations:**

- Optional logging with timestamp, UID, GID, and PID for each client action.
- Deterministic behavior: same inputs lead to the same state.

---

### 4. Information Disclosure

**Threat:** Unauthorized access to sensitive client data.

**Risks:**

- Leaking another client's registration state

**Mitigations:**

- Socket is only accessible to the `wdclients` group.
- Secrets are stored only in RAM and are not persisted by the broker.
- Broker runs as an unprivileged user to minimize the leakage surface.
- Even if a `clientID` is compromised, its use requires the `pid` to match the original registering process, which is considered highly unlikely in practice.

---

### 5. Denial of Service (DoS)

**Threat:** Causing broker malfunction or triggering false watchdog expiry.

**Risks:**

- Flooding the broker with excessive `REGISTER` or `PING` requests to exhaust resources.
- Registering a fake client and letting it timeout.
- Overloading the socket to block legitimate clients.
- Overwriting an active socket used by a running instance.

**Mitigations:**

- Only processes belonging to the `wdclients` group or `root` can access the Unix domain socket. This prevents unauthorized users from interacting with the broker.

---

### 6. Elevation of Privilege

**Threat:** Unauthorized user gains higher access via broker.

**Risks:**

- Misusing broker to gain root access or write to `/dev/watchdog`.

**Mitigations:**

- Broker starts as root only to open `/dev/watchdog`, then drops to an unprivileged user.
- Only feeds `/dev/watchdog`; no general-purpose execution or file access.
- Socket group restriction prevents non-members from connecting.

---

## Conclusion

The `wd-broker` daemon is designed to minimize trust while offering a robust watchdog supervision service. The system assumes local attackers with limited privileges and hardens against them through a combination of:

- **Access control**: Restricting socket access to the `wdclients` group.
- **Stateless protocol**: Ensuring predictable behavior and minimizing resource usage.
- **Privilege dropping**: Running as an unprivileged user after initialization.
- **Socket safety checks**: Verifying and recreating stale sockets to prevent interference.

By explicitly defining threats using STRIDE, the design remains transparent and auditable for safety-critical contexts.

---

## Status

| Threat                     | Mitigation                    | Status              |
|----------------------------|-------------------------------|---------------------|
| Spoofing                   | PID verification              | Implemented         |
| Tampering                  | Stateless protocol            | Implemented         |
| Information Disclosure     | Socket access control         | Implemented         |
| Denial of Service (DoS)    | Limited socket access         | Implemented         |
| Elevation of Privilege     | Privilege dropping            | Implemented         |
| Logging (UID, GID, PID)    | Optional logging              | Planned             |

---

*Last updated: 2025-05-01*

