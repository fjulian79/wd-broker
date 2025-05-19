# Threat Model for wd-broker

This document describes the threat model for the `wd-broker` daemon using the STRIDE methodology. The goal is to identify potential security risks and clarify the design decisions that mitigate or acknowledge those risks. This analysis assumes `wd-broker` may be deployed in safety-relevant environments where false positives (accidental reboots) and false negatives (system hangs not triggering a reboot) both carry critical weight.

For details on the STRIDE methodology, see [Wikipedia – STRIDE model](https://en.wikipedia.org/wiki/STRIDE_model).

## System Overview

`wd-broker` is a user-space daemon responsible for supervising access to the hardware watchdog on embedded Linux systems. It listens on a Unix Domain Socket (UDS) for client applications that must:

- Register themselves with a name and timeout, (if configured) within a given time limit
- Once registered, periodically ping the broker to prove their fitness
- Deregister when shutting down cleanly

If any ..
- active client fails to ping within its declared timeout
- announced client fails to register within the configured time limit

.. the broker will stop feeding the hardware watchdog, resulting in a system reboot.

## Assumptions

- Only users in a dedicated group and root can access the Unix socket.
- It is acceptable for `root` to be able to unregister any client, assuming that root already has full system control. This is relevant during development and test scenarios, and acceptable in production as well.
- Clients are responsible for managing their registration state securely.
- The system assumes the integrity of the operating system and the `wd-clients` group. If a member of the `wd-clients` group is compromised, the broker cannot prevent misuse.
- The system should allow reboots in case of actual application failures (e.g., deadlocks), but resist intentional misuse.
- The broker is started as `root` but drops privileges after initialization (see below).

## Privilege Management

`wd-broker` requires elevated privileges only during startup:

- To open `/dev/watchdog`, which typically requires root
- Depending on the actual socket path, to create the Unix socket with the correct permissions and group ownership

After completing these steps, the broker **drops privileges** using `setgid()` and `setuid()` to run as an unprivileged user within the `wd-clients` group. This ensures that even if the broker process is compromised during runtime, the system impact is limited.

To avoid disrupting a running instance, the broker performs a safety check before binding to the socket path. If the socket already exists, it attempts to connect to it. If a process is actively listening, the broker aborts with a clear error message. If the connection fails (e.g., with ECONNREFUSED), the socket is considered stale, and the broker may safely remove and recreate it.

This approach reduces the attack surface and aligns with least-privilege principles.

## STRIDE Analysis

### 1. Spoofing (Identity Forgery)

**Threat:** A malicious process attempts to impersonate a legitimate client by using their `clientID` or `name`.

**Mitigations:**
- Only processes belonging to the trusted group `wd-clients` can connect to the socket (enforced by Unix file permissions).
  - As a result no extra mitigation is planned against malicious clients using client names or ID's 
  - This is considered a design choice, as all members of the `wd-clients` group are trusted.
- Each client is identified by its `name` and `pid` during registration resulting in a unique `clientID` which is used for all further communication.
- The broker verifies the `pid` of the process sending commands (e.g., `PING`, `UNREGISTER`) to ensure it matches the PID of the process that originally registered the `clientID`.
- The broker runs with dropped privileges after initialization to reduce spoofing risks if exploited.
- Since the broker cannot enforce secure handling of client state on the client side, a compromised client can continue to interact with the broker using its own `clientID`. This limitation is accepted by design: all members of the `wd-clients` group are considered trusted, and authentication serves as a lightweight safeguard against accidental misuse or implementation errors.

---

### 2. Tampering (Data Manipulation)

**Threat:** Malicious modification of data in transit, client state, or other system data.
**Threat:** An attacker could modify the configuration file to weaken security (e.g., increase timeouts, allow unauthorized clients, or disable safeguards).

**Risks:**

- Modifying ping payloads to falsify liveness
- Corrupting internal state to keep dead clients alive
- Unregistering other clients
- If the config file is writable by unauthorized users, they could alter broker behavior or bypass controls.

**Mitigations:**

- The broker checks that the given socket path is a valid Unix domain socket and that it is owned by the `wd-clients` group to prevent tampering (e.g. symlink attacks).
- The broker verifies the `pid` of the process sending `PING` commands to ensure it matches the original registering process.
- The socket protocol is request-response per connection with stateless handling.
- Broker memory (client table) is private to the process; no external tampering is possible.
- Dropping privileges limits the potential damage if the process is compromised.
- The configuration file is owned by a dedicated service user (e.g., `wd-broker`) and is not writable by other users (`chmod 640` or stricter). This prevents unauthorized modifications.
- The broker checks the config file ownership and permissions on startup, refusing to run if they are too permissive. This leads to not feeding the hardware watchdog, which will cause a system reboot. This avoids silently running based on a potentially unsafe config file.

---

### 3. Repudiation (Denying Responsibility)

Repudiation is not applicable. Clients do not authenticate, and the broker has no expectation of accountable actions from them. Members of the `wd-clients` group are considered trusted, and the broker does not enforce any authentication mechanisms. However, suspicious behavior is logged along with the context (timestamp, pid, client names, ...) by the broker for auditing purposes.

---

### 4. Information Disclosure

**Threat:** Unauthorized access to sensitive client data.
**Threat:** The configuration file may contain sensitive information (e.g., client names, paths) that could aid an attacker.

**Risks:**

- Leaking another client's data (clientID, timeout, name, pid)
- Accidental misconfiguration or deployment errors could introduce vulnerabilities.

**Mitigations:**

- Socket is only accessible to the `wd-clients` group.
- Data is only stored in RAM and is not persisted by the broker.
- Broker runs as an unprivileged user to minimize the leakage surface.
- Even if a `clientID` is compromised, it's use requires the `pid` to match the original registering process, which is considered highly unlikely in practice.
- Note that the client can disable the PID check, but only once during registration. If it does, only its own `clientID` may be used by others. This limits the impact to that client's scope.
- The configuration file is owned by a dedicated service user (e.g., `wd-broker`) and is not writable by other users (`chmod 640` or stricter). This prevents unauthorized access to sensitive information.

---

### 5. Denial of Service (DoS)

**Threat:** Causing broker malfunction or triggering false watchdog expiry.
**Threat:** Deleting or corrupting the configuration file could prevent the broker from starting or cause it to run with unsafe defaults.

**Risks:**

- Flooding the broker with excessive `REGISTER` or `PING` requests to exhaust resources.
- Registering a fake client and letting it timeout.
- When `unique_clients` or `strict_clients` is enabled, a malicious client could register with the same name as a legitimate client, causing the broker to reject the legitimate client.
- Overloading the socket to block legitimate clients.
- Overwriting an active socket used by a running instance.

**Mitigations:**

- Only processes belonging to the `wd-clients` group or `root` can access the Unix domain socket. This prevents unauthorized users from interacting with the broker. 
  - As a result no extra mitigation is planned against malicious clients occupying client names of legitimate clients when `unique_clients` or `strict_clients` is enabled.
  - This is considered a design choice, as all members of the `wd-clients` group are trusted.
- The configuration file is owned by a dedicated service user (e.g., `wd-broker`) and is not writable by other users (`chmod 640` or stricter). This prevents unauthorized modifications.
- On startup, the broker checks config file ownership and permissions, refusing to run if they are too permissive. This is accepted as a potential risk, but modifications to config file depend on appropriate privileges. If the attacker has such privileges, they can already do significant damage.

---

### 6. Elevation of Privilege

**Threat:** Unauthorized user gains higher access via broker.

**Risks:**

- Misusing broker to gain root access or write to `/dev/watchdog`.

**Mitigations:**

- Broker starts as root to open `/dev/watchdog` and other needed resources, then drops to an unprivileged user.
- Only feeds `/dev/watchdog`; no general-purpose execution or file access.
- Socket group restriction prevents non-members from connecting.

## Conclusion

The `wd-broker` daemon is designed to minimize trust while offering a robust watchdog supervision service. The system assumes local attackers with limited privileges and hardens against them through a combination of:

- **Access control**: Restricting socket access to the `wd-clients` group.
- **Stateless protocol**: Ensuring predictable behavior and minimizing resource usage.
- **Privilege dropping**: Running as an unprivileged user after initialization.
- **Socket safety checks**: Verifying and recreating stale sockets to prevent interference.
- **Memory safety**: The broker avoids dynamic memory allocation entirely. All state is kept in fixed-size structures in RAM, eliminating risks related to heap corruption, fragmentation, or allocation failure.

By explicitly defining threats using STRIDE, the design remains transparent and auditable for safety-critical contexts.

---

## Status

| Threat                     | Mitigation                                                     | Status      |
|----------------------------|----------------------------------------------------------------|-------------|
| Spoofing                   | PID verification                                               | Implemented |
| Tampering                  | Stateless protocol, socket and config file integrity checks    | Implemented |
| Repudiation                | Not applicable (no auth); suspicious behavior is logged        | n.a.        |
| Information Disclosure     | Socket access control                                          | Implemented |
| Denial of Service (DoS)    | Limited socket access, stale socket detection                  | Implemented |
| Elevation of Privilege     | Privilege dropping                                             | Implemented |

---

*Last updated: 2025-05-19*
