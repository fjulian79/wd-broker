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

- Only users in a dedicated group (e.g. `wdclients`) can access the Unix socket.

- It is acceptable for `root` to be able to unregister any client regardless of token validity, assuming that root already has full system control. This is relevant during development and test scenarios, and acceptable in production as well.

- Clients are responsible for managing their client ID and authentication token securely.

- The system should allow reboots in case of actual application failures (e.g. deadlocks), but resist intentional misuse.

- The broker is started as `root` but drops privileges after initialization (see below).

## Privilege Management

`wd-broker` requires elevated privileges only during startup:

- To open `/dev/watchdog`, which typically requires root
- To create the Unix socket with the correct permissions and group ownership

After completing these steps, the broker **drops privileges** using `setgid()` and `setuid()` to run as an unprivileged user within the `wdclients` group. This ensures that even if the broker process is compromised during runtime, the system impact is limited.

To avoid disrupting a running instance, the broker performs a safety check before binding to the socket path. If the socket already exists, it attempts to connect to it. If a process is actively listening, the broker aborts with a clear error message. If the connection fails (e.g. with ECONNREFUSED), the socket is considered stale, and the broker may safely remove and recreate it.

This approach reduces the attack surface and aligns with least-privilege principles.

---

## STRIDE Analysis

### 1. Spoofing (Identity Forgery)

**Note:** As stated in the assumptions section, spoofing by `root` is not considered a threat, as root has full system control.

**Mitigations:**

- Each client receives a random `clientId` and an HMAC-based `token` during registration.
- The token is bound to the client's `linux_uid` and is verified on every command.
- Only processes belonging to the trusted group `wdclients` can connect to the socket (Unix file permissions).
- Broker runs with dropped privileges after initialization to reduce spoofing risks if exploited.
- The token mechanism is effective against spoofing attempts from other processes within the `wdclients` group, but not against a compromised client that already possesses a valid `clientId` and `token`.
- Since the broker cannot enforce secure token handling on the client side, a compromised client can continue to interact with the broker using its own credentials.
- This limitation is accepted by design: all members of the `wdclients` group are considered trusted, and token-based authentication serves as a lightweight safeguard against accidental misuse or implementation errors.
- No official client library or implementation is provided, which means clients must take responsibility for correctly managing their registration state and credentials.

- Each client receives a random `clientId` and an HMAC-based `token` during registration.
- The token is bound to the client's `linux_uid` and is verified on every command.
- Only processes belonging to the trusted group `wdclients` can connect to the socket (Unix file permissions).
- Broker runs with dropped privileges after initialization to reduce spoofing risks if exploited.

### 2. Tampering (Data Manipulation)

**Threat:** Malicious modification of data in transit or client state.

**Risks:**

- Modifying ping payloads to falsify liveness
- Corrupting internal state to keep dead clients alive

**Mitigations:**

- The socket protocol is request-response per connection with stateless handling
- Tokens are cryptographically verified and bound to both `clientId` and `linux_uid`
- Broker memory (client table) is private to the process; no external tampering possible
- Privilege drop ensures internal state cannot be overwritten by unprivileged processes

### 3. Repudiation (Denying Responsibility)

**Threat:** A client claims it did not perform a certain action (e.g. deregistering).

**Risks:**

- False claims of broker misbehavior

**Mitigations:**

- Optional logging with timestamp, UID, GID, PID for each client action
- Deterministic behavior: same inputs lead to same state

### 4. Information Disclosure

**Threat:** Unauthorized access to sensitive client data

**Risks:**

- Leaking another client's `clientId` and `token`

**Mitigations:**

- Socket is only accessible to the `wdclients` group
- Tokens are never sent over insecure channels or persisted by broker
- Secrets are stored only in RAM and regenerated on each broker restart
- Broker runs as unprivileged user to minimize leakage surface

### 5. Denial of Service (DoS)

**Threat:** Causing broker malfunction or triggering false watchdog expiry

**Risks:**

- Flooding with REGISTER or PING to exhaust resources
- Registering a fake client and letting it timeout
- Overloading the socket to block real clients
- Overwriting an active socket used by a running instance

**Mitigations:**

- Maximum number of clients is limited
- Per-client registration limits (e.g. 1 per UID)
- No rate limiting is applied, as performance during system startup takes priority. 
    - The broker is designed to handle a high rate of incoming connections efficiently and to fail fast on invalid or unexpected inputs, rather than attempting to throttle or queue them. 
    - This design choice ensures rapid availability of all components after boot and avoids unnecessary complexity.
- The broker drops privileges after initialization to prevent that even if the broker is overloaded or misused, access to /dev/watchdog cannot be escalated or abused.
- Broker verifies socket state on startup: it only removes and recreates the socket if no process is listening (stale socket); otherwise it aborts to prevent accidental interference.

### 6. Elevation of Privilege

**Threat:** Unauthorized user gains higher access via broker

**Risks:**

- Misusing broker to gain root access or write to /dev/watchdog

**Mitigations:**

- Broker starts as root only to open `/dev/watchdog`, then drops to unprivileged user
- Only feeds /dev/watchdog; no general-purpose execution or file access
- Socket group restriction prevents non-members from connecting

---

## Conclusion

The `wd-broker` daemon is designed to minimize trust while offering a robust watchdog supervision service. The system assumes local attackers with limited privileges and hardens against them through a combination of access control, cryptographic tokens, memory-only secrets, strict protocol validation, socket safety checks, and privilege dropping.

A stronger challenge-response authentication mechanism was considered but ultimately deliberately avoided. Implementing such a system would require client-side encryption capabilities, secure key management, and additional protocol complexity. These requirements would have necessitated the introduction of a client library. This would have raised the question of which language or runtime to target. A C/C++ implementation alone was deemed insufficient and would have limited interoperability and usability across diverse environments. Instead, wd-broker pursues a minimalist and uncluttered design, sacrificing some cryptographic accuracy for clarity, portability, and ease of integration.

While the token mechanism increases robustness against spoofing, it does not protect against misuse by clients that are already trusted and possess valid credentials. This is a deliberate trade-off, as all `wdclients` members are assumed to be cooperative. Since there is no reference client implementation, secure handling of `clientId` and `token` is the responsibility of each client.

By explicitly defining threats using STRIDE, the design remains transparent and auditable for safety-critical contexts.

---

## Status

| Item | Implemented | Commit |
|------|-------------|--------|
| Max client limit enforcement | ✅ | f9714086c52b996ce42d1ea769df967992def212 |
| Logging (UID, GID, PID on actions, ...) | ⬜ | — |
| UDS socket with group-based access control | ⬜ | — |
| Socket reuse detection and stale cleanup | ⬜ | — |
| Privilege dropping after startup | ⬜ | — |
| clientId entropy upgrade (256 bit) | ⬜ | — |
| clientId uniqueness | ⬜ | — |
| Per-UID client registration limit | ⬜ | — |
| Token generation and HMAC verification | ⬜ | — |

---

*Last updated: 2025-04-20*

