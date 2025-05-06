/*
 * wd-broker, a minimalistic watchdog supervisor daemon that acts as the single
 * source of truth for hardware watchdog feeding on embedded Linux systems.
 *
 * Copyright (C) 2025 Julian Friedrich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This project is hosted on GitHub:
 *   https://github.com/fjulian79/wd-broker
 * Please feel free to file issues, open pull requests, or contribute there.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <linux/watchdog.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "config.h"

#define STR_HELPER(x)              #x
#define STR(x)                     STR_HELPER(x)
#define WD_HW_TIMEOUT_DEFAULT_S    10
#define WD_HW_TIMEOUT_MIN_S        WD_HW_TIMEOUT_DEFAULT_S
#define WD_HW_TIMEOUT_MAX_S        60
#define WD_CLIENT_NAME_LEN         64
#define WD_CLIENT_NAME_FMT_LEN_STR "63" // cant use (CLIENT_NAME_LEN - 1) here
#define WD_REGISTER_SCANF_FORMAT   "%" WD_CLIENT_NAME_FMT_LEN_STR "s %d %15s %15s"
#define WD_CLIENTID_LEN            17 // 16 hex digits + null terminator
#define WD_CLIENTID_FMT_LEN        16 // Must be a number cant use CLIENTID_LEN - 1
#define WD_CLIENTID_FMT_STR        STR(WD_CLIENTID_FMT_LEN)
#define WD_CLIENTID_SCANF_FORMAT   "%" WD_CLIENTID_FMT_STR "s %15s"
#define WD_CLIENT_TIMEOUT_MIN_MS   (5 * 1000)
#define WD_CLIENT_TIMEOUT_MAX_MS   (300 * 1000)
#define WD_CLIENT_IDENTIFIED       0
#define WD_CLIENT_PID_MISMATCH     -1
#define WD_CLIENT_NOT_FOUND        -2

typedef struct {
    char            clientID[WD_CLIENTID_LEN];
    char            name[WD_CLIENT_NAME_LEN];
    pid_t           pid;
    unsigned int    timeout_ms;
    struct timespec last_ping;
    bool            active;
    bool            checkPID;
} client_t;

int           wd_timeout_s = WD_HW_TIMEOUT_DEFAULT_S;
char         *socket_path = SOCKET_PATH_DEFAULT;
volatile bool running = true;
bool          daemonize = false;

void print_help(const char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\nOptions:\n");
    printf("  --help                    Show this help message and exit\n");
    printf("  --version                 Show version information and exit\n");
    printf("  --daemonize               Run as a daemon (default: false)\n");
    printf("  --wd-timeout <seconds>    Set hardware watchdog timeout (default: %d seconds)\n",
           WD_HW_TIMEOUT_DEFAULT_S);
    printf("                            Must be between %d and %d seconds\n", WD_HW_TIMEOUT_MIN_S,
           WD_HW_TIMEOUT_MAX_S);
    printf("  --socket-path <path>      Set the Unix domain socket path (default: %s)\n",
           SOCKET_PATH_DEFAULT);
    printf("  --service-user <user>     Set the service user to drop privileges to (default: %s)\n",
           SERVICE_USER_DEFAULT);
    printf("                            ATTENTION: Must not be 'root'\n");
    printf("  --syslog-facility <name>  Set syslog facility when running as a daemon\n");
    printf("                            Supported values: LOG_DAEMON (default), LOG_USER,\n");
    printf("                            LOG_LOCAL0 through LOG_LOCAL7\n");
    printf("  --no-watchdog             Disable hardware watchdog (test mode, default: false)\n");
    printf("\nExamples:\n");
    printf("  %s --wd-timeout 15 --socket-path /run/your-own.sock\n", progname);
    printf("  %s --daemonize --service-user youruser\n", progname);
    printf("\n");
}

void log_message(int priority, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    /* If we are running as a daemon, use syslog to log messages, otherwise use
     * stdout to have the logs in the console. */
    if (!daemonize) {
        FILE          *fd = (priority <= LOG_WARNING ? stdout : stderr);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm tmb;
        localtime_r(&tv.tv_sec, &tmb);

        const char *prio_str;
        switch (priority) {
            case LOG_EMERG:
                prio_str = "EMERG ";
                break;
            case LOG_ALERT:
                prio_str = "ALERT ";
                break;
            case LOG_CRIT:
                prio_str = "CRIT  ";
                break;
            case LOG_ERR:
                prio_str = "ERROR ";
                break;
            case LOG_WARNING:
                prio_str = "WARN  ";
                break;
            case LOG_NOTICE:
                prio_str = "NOTICE";
                break;
            case LOG_INFO:
                prio_str = "INFO  ";
                break;
            case LOG_DEBUG:
                prio_str = "DEBUG ";
                break;
            default:
                prio_str = "UNDEF ";
                break;
        }

        char timestr[32];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tmb);
        fprintf(fd, "%s.%06ld [%s] ", timestr, (long)tv.tv_usec, prio_str);
        vfprintf(fd, fmt, ap);
        fprintf(fd, "\n");
        fflush(fd);
    } else {
        vsyslog(priority, fmt, ap);
    }

    va_end(ap);
}

void write_str(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    if (vdprintf(fd, fmt, ap) < 0) {
        log_message(LOG_ERR, "vdprintf: %s", strerror(errno));
    }

    va_end(ap);
}

void make_clientID(char *clientID_out) {
    static const char hex[] = "0123456789abcdef";
    size_t            raw[WD_CLIENTID_LEN - 1];
    bool              have_raw = false;

    /* Try to read random bytes from /dev/urandom */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, raw, sizeof(raw));
        close(fd);

        if (n == sizeof(raw)) {
            have_raw = true;
        }
    }

    /* If /dev/urandom is not available, use random() to generate a pseudo-random clientID */
    if (!have_raw) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        unsigned int seed = (unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid() ^ getppid());

        srandom(seed);
        for (size_t i = 0; i < WD_CLIENTID_LEN - 1; ++i) {
            raw[i] = random() & 0xFF;
        }
    }

    /* Convert the raw bytes to a hex string */
    for (size_t i = 0; i < WD_CLIENTID_LEN - 1; ++i) {
        clientID_out[i] = hex[raw[i] & 0x0F];
    }

    clientID_out[WD_CLIENTID_LEN - 1] = '\0';
}

bool parse_clientID(const char *buf, const char *cmd_prefix, int sock, char *out_id) {
    char clientID_raw[WD_CLIENTID_LEN];
    char extra[16];
    int  n = sscanf(buf + strlen(cmd_prefix), WD_CLIENTID_SCANF_FORMAT, clientID_raw, extra);

    /* Try to match exactly one argument, and reject trailing garbage */
    if (n != 1) {
        write_str(sock, "ERROR invalid syntax\n");
        return false;
    }

    /* Check if clientID is a valid hex string */
    size_t id_len = strlen(clientID_raw);
    if (id_len != WD_CLIENTID_FMT_LEN) {
        write_str(sock, "ERROR invalid clientID length\n");
        return false;
    }

    /* Check if clientID is a valid hex string and enforce lower case */
    for (size_t i = 0; i < id_len; ++i) {
        if (!isxdigit((unsigned char)clientID_raw[i])) {
            write_str(sock, "ERROR invalid clientID format\n");
            return false;
        }
        out_id[i] = (char)tolower((unsigned char)clientID_raw[i]);
    }

    out_id[id_len] = '\0';
    return true;
}

int32_t get_clientInstance(client_t *clients, const char *clientID, pid_t pid, client_t **client) {
    for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
        if (clients[i].active && strncmp(clients[i].clientID, clientID, WD_CLIENTID_FMT_LEN) == 0) {
            *client = &clients[i];
            /* Check if the clientID is registered with a PID the checkPID flag is here for
             * future use to allow clients to turn of the pid check. But this should be be
             * allowed only when they register to avoid abuse. */
            if (clients[i].checkPID == false || clients[i].pid == pid) {
                return WD_CLIENT_IDENTIFIED;
            } else {
                return WD_CLIENT_PID_MISMATCH;
            }
        }
    }
    return WD_CLIENT_NOT_FOUND;
}

void get_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

unsigned int ms_since(struct timespec *then) {
    struct timespec now;
    get_now(&now);
    return (now.tv_sec - then->tv_sec) * 1000 + (now.tv_nsec - then->tv_nsec) / 1000000;
}

void signal_handler(int sig) {
    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
    running = false;
}

void handle_command(int client_sock, client_t *clients) {
    client_t    *pClient = {0};
    int32_t      ret = 0;
    char         buf[128] = {0};
    ssize_t      len = 0;
    struct ucred creds;
    socklen_t    socklen = sizeof(creds);

    if (getsockopt(client_sock, SOL_SOCKET, SO_PEERCRED, &creds, &socklen) == -1) {
        write_str(client_sock, "ERROR reading peercred\n");
        log_message(LOG_ERR, "getsockopt: %s", strerror(errno));
        return;
    }

    len = read_with_timeout(client_sock, buf, sizeof(buf) - 1, true);
    if (len <= 0) {
        write_str(client_sock, "ERROR no input\n");
        return;
    }
    buf[len] = '\0';

    if (strncmp(buf, CMD_REGISTER, strlen(CMD_REGISTER)) == 0) {
        char         name[WD_CLIENT_NAME_LEN];
        unsigned int tmp_timeout = 0;
        char         opt_flag[16] = {0};
        char         extra[16] = {0}; // catches unexpected extra input
        int          n = 0;
        bool         checkPID = true;

        /* Try to match exactly two arguments, and reject trailing garbage */
        n = sscanf(buf + strlen(CMD_REGISTER), WD_REGISTER_SCANF_FORMAT, name, &tmp_timeout,
                   opt_flag, extra);
        if (n < 2 || n > 3) {
            write_str(client_sock, "ERROR invalid REGISTER syntax\n");
            return;
        }

        /* If 'ignorepid' is given, skip PID check for this client */
        if (n == 3) {
            if (strcmp(opt_flag, "ignorepid") == 0) {
                checkPID = false;
            } else {
                write_str(client_sock, "ERROR invalid REGISTER syntax\n");
                return;
            }
        }

        /* Validate timeout range */
        if (tmp_timeout < WD_CLIENT_TIMEOUT_MIN_MS || tmp_timeout > WD_CLIENT_TIMEOUT_MAX_MS) {
            write_str(client_sock, "ERROR invalid timeout\n");
            return;
        }

        /* Reject empty or clearly broken names */
        for (size_t i = 0; i < strlen(name); ++i) {
            if (!isprint(name[i]) || isspace(name[i])) {
                write_str(client_sock, "ERROR invalid client name\n");
                return;
            }
        }

        /* Register the client */
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (!clients[i].active) {
                clients[i].active = true;
                strncpy(clients[i].name, name, sizeof(clients[i].name) - 1);
                clients[i].pid = creds.pid;
                clients[i].checkPID = checkPID;
                clients[i].timeout_ms = tmp_timeout;
                get_now(&clients[i].last_ping);
                make_clientID(clients[i].clientID);
                log_message(
                    LOG_INFO,
                    "Client '%s' (PID %d) registered (timeout %d ms, pidCheck %s, clientID %s)",
                    clients[i].name, (int)clients[i].pid, clients[i].timeout_ms,
                    checkPID ? "enabled" : "disabled", clients[i].clientID);
                write_str(client_sock, "OK %s\n", clients[i].clientID);
                return;
            }
        }

        /* If we reach this point, all client slots are taken */
        write_str(client_sock, "ERROR too many clients\n");

    } else if (strncmp(buf, CMD_PING, strlen(CMD_PING)) == 0) {
        char clientID[WD_CLIENTID_LEN] = {0};

        if (!parse_clientID(buf, CMD_PING, client_sock, clientID)) {
            return;
        }

        ret = get_clientInstance(clients, clientID, creds.pid, &pClient);
        switch (ret) {
            case WD_CLIENT_IDENTIFIED:
                get_now(&pClient->last_ping);
                write_str(client_sock, "OK\n");
                return;
            case WD_CLIENT_PID_MISMATCH:
                write_str(client_sock, "ERROR wrong PID\n");
                log_message(LOG_WARNING, "Client '%s', known as PID %d has sent PING from PID %d",
                            pClient->name, (int)pClient->pid, (int)creds.pid);
                return;
            default:
                write_str(client_sock, "ERROR unknown clientID\n");
                return;
        }

    } else if (strncmp(buf, CMD_UNREGISTER, strlen(CMD_UNREGISTER)) == 0) {
        char clientID[WD_CLIENTID_LEN] = {0};

        if (!parse_clientID(buf, CMD_UNREGISTER, client_sock, clientID)) {
            return;
        }

        ret = get_clientInstance(clients, clientID, creds.pid, &pClient);
        switch (ret) {
            case WD_CLIENT_IDENTIFIED:
                log_message(LOG_INFO, "Client '%s' (PID %d) unregistered (clientID=%s)",
                            pClient->name, (int)pClient->pid, pClient->clientID);
                pClient->active = false;
                write_str(client_sock, "OK\n");
                return;
            case WD_CLIENT_PID_MISMATCH:
                if (creds.uid == 0) {
                    pClient->active = false;
                    write_str(client_sock, "OK\n");
                    log_message(LOG_INFO, "Client '%s' (PID %d) unregistered by root (clientID=%s)",
                                pClient->name, (int)pClient->pid, pClient->clientID);
                    return;
                } else {
                    write_str(client_sock, "ERROR wrong PID\n");
                    log_message(LOG_WARNING,
                                "Client '%s', known as PID %d has sent UNREGISTER from PID %d",
                                pClient->name, (int)pClient->pid, (int)creds.pid);
                    return;
                }
            default:
                write_str(client_sock, "ERROR unknown clientID\n");
                return;
        }

    } else if (strncmp(buf, CMD_STATUS, strlen(CMD_STATUS)) == 0) {
        char extra[32];
        if (sscanf(buf + strlen(CMD_STATUS), " %31s", extra) == 1) {
            write_str(client_sock, "ERROR invalid syntax\n");
            return;
        }

        if (creds.pid != 0 && creds.uid != 0) {
            write_str(client_sock, "ERROR no permission\n");
            log_message(LOG_WARNING, "Client with PID %d tried to list clients", (int)creds.pid);
            return;
        }

        write_str(client_sock, "daemon_version=%s\n", PACKAGE_VERSION);
        write_str(client_sock, "protocol_version=%s\n", SOCKET_PROT_VERSION);
        write_str(client_sock, "wd_timeout_s=%d\n", wd_timeout_s);
        size_t active_clients = 0;
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (clients[i].active) {
                active_clients++;
            }
        }
        write_str(client_sock, "active_clients=%d\n", active_clients);
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (clients[i].active) {
                write_str(client_sock, "%s %d %s %u %d\n", clients[i].clientID, (int)clients[i].pid,
                          clients[i].name, clients[i].timeout_ms, clients[i].checkPID ? 1 : 0);
            }
        }
    } else if (strncmp(buf, CMD_VERSION, strlen(CMD_VERSION)) == 0) {
        char extra[32];
        if (sscanf(buf + strlen(CMD_VERSION), " %31s", extra) == 1) {
            write_str(client_sock, "ERROR invalid syntax\n");
            return;
        }

        write_str(client_sock, "daemon_version=%s\n", PACKAGE_VERSION);
        write_str(client_sock, "protocol_version=%s\n", SOCKET_PROT_VERSION);
    } else {
        write_str(client_sock, "ERROR unknown command\n");
    }
}

int parse_syslog_facility(const char *str) {
    if (str == NULL) {
        return LOG_DAEMON;
    }

    if (strcmp(str, "LOG_USER") == 0) {
        return LOG_USER;
    } else if (strcmp(str, "LOG_DAEMON") == 0) {
        return LOG_DAEMON;
    } else if (strcmp(str, "LOG_LOCAL0") == 0) {
        return LOG_LOCAL0;
    } else if (strcmp(str, "LOG_LOCAL1") == 0) {
        return LOG_LOCAL1;
    } else if (strcmp(str, "LOG_LOCAL2") == 0) {
        return LOG_LOCAL2;
    } else if (strcmp(str, "LOG_LOCAL3") == 0) {
        return LOG_LOCAL3;
    } else if (strcmp(str, "LOG_LOCAL4") == 0) {
        return LOG_LOCAL4;
    } else if (strcmp(str, "LOG_LOCAL5") == 0) {
        return LOG_LOCAL5;
    } else if (strcmp(str, "LOG_LOCAL6") == 0) {
        return LOG_LOCAL6;
    } else if (strcmp(str, "LOG_LOCAL7") == 0) {
        return LOG_LOCAL7;
    }

    /* Can't user fatal_error(...) here because the compiler complains about having a
     * no return statement in a non-void function */
    fprintf(stderr, "Error: Only LOG_USER, LOG_DAEMON and LOG_LOCAL* are supported\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    client_t           clients[WD_MAX_CLIENTS] = {0};
    int                watchdog_fd = -1;
    bool               watchdog_enabled = true;
    bool               all_ok = true;
    int                server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    struct stat        st;
    int                timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec  timer_spec = {0};
    const char        *service_user = SERVICE_USER_DEFAULT;
    int                facility = LOG_DAEMON;
    bool               started_as_root = geteuid() == 0 ? true : false;

    static struct option long_options[] = {
        {"daemonize", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"no-watchdog", no_argument, 0, 'n'},
        {"service-user", required_argument, 0, 'u'},
        {"socket-path", required_argument, 0, 'p'},
        {"syslog-facility", required_argument, 0, 's'},
        {"wd-timeout", required_argument, 0, 't'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                daemonize = true;
                break;
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            case 'n':
                watchdog_enabled = false;
                break;
            case 'p':
                socket_path = optarg;
                break;
            case 's':
                facility = parse_syslog_facility(optarg);
                break;
            case 't':
                wd_timeout_s = atoi(optarg);
                break;
            case 'u':
                service_user = optarg;
                break;
            case 'v':
                printf("wd-broker v%s\n", PACKAGE_VERSION);
                return 0;
            default:
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (wd_timeout_s < WD_HW_TIMEOUT_MIN_S || wd_timeout_s > WD_HW_TIMEOUT_MAX_S) {
        fatal_error("Invalid watchdog timeout: must be between %d and %d seconds.\n",
                    WD_HW_TIMEOUT_MIN_S, WD_HW_TIMEOUT_MAX_S);
    }

    /* We want to tick one second faster then the hardware watchdog to have a safety margin
     * for scheduling jitter and high CPU loads. The minimum value of wd_timeout_s is
     * LOOP_INTERVAL_MIN_MS and checkd above so we are save to take one second off */
    timer_spec.it_interval.tv_sec = wd_timeout_s - 1;
    timer_spec.it_value.tv_sec = timer_spec.it_interval.tv_sec;
    if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) == -1) {
        fatal_errno("timerfd_settime failed");
    }

    struct passwd *pw = getpwnam(service_user);
    if (!pw) {
        fatal_error("Service user '%s' does not exist.\n", service_user);
    }
    if (pw->pw_uid == 0) {
        fatal_error("Refusing to drop privileges to root, use a different service user.\n");
    }

    /* Do not trust the given socket path, check if it is a socket and remove it if it is */
    if (lstat(socket_path, &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            fatal_error("Cannot use '%s'. File exists but is not a socket.\n"
                        "Hint: Remove the file or choose a different path.",
                        socket_path);
        }

        int probe_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (probe_fd < 0) {
            fatal_errno("Could not create socket to test for active broker");
        }

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        if (connect(probe_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            close(probe_fd);
            fatal_error("Active wd-broker instance detected at '%s'.\n", socket_path);
        }

        if (errno != ECONNREFUSED) {
            close(probe_fd);
            fatal_errno("Unexpected error when probing existing socket");
        }

        close(probe_fd);

        if (unlink(socket_path) == 0) {
            printf("Removed stale socket at '%s'\n", socket_path);
        } else {
            fatal_errno("Could not remove stale socket file");
        }
    }

    /* socket path is fine, create it */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno == EACCES) {
            fprintf(stderr, "Error: Permission denied to bind to socket path '%s'\n", socket_path);
            if (!started_as_root) {
                fprintf(stderr, "Please run as root or use a different socket path\n");
            }
        } else {
            fatal_error("Failed to bind to socket path '%s'\n", socket_path);
        }
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock, 5) == -1) {
        fatal_errno("Could not listen on the server socket");
    }

    /* Set socket permissions to the service user and group */
    if (chmod(socket_path, 0660) == -1) {
        fatal_errno("Could not set permissions on socket file");
    }
    if (started_as_root && chown(socket_path, pw->pw_uid, pw->pw_gid) == -1) {
        fatal_errno("Could not set permissions on socket file");
    }

    /* Opening the watchdog device is not mandatory, but if it is enabled, we need to
     * open it as root. We also need to set the timeout here, because the watchdog
     * driver does not support setting the timeout after opening the device.
     * We want to do this as late as pissible avoiding to open the device
     * in case there is anything wring with the given user parameters or the client
     * socket. */
    if (watchdog_enabled) {
        if (!started_as_root) {
            fatal_error("wd-broker must be started as root!\n");
        }

        watchdog_fd = open("/dev/watchdog", O_WRONLY);
        if (watchdog_fd == -1) {
            fatal_error("Failed to open /dev/watchdog: %s\n", strerror(errno));
        }
        if (ioctl(watchdog_fd, WDIOC_SETTIMEOUT, &wd_timeout_s) == -1) {
            fatal_error("Failed to set watchdog timeout: %s\n", strerror(errno));
        }
    }

    /* Drop privileges to the service user if we started as root */
    if (started_as_root) {
        if (setgroups(0, NULL) == -1) {
            fatal_errno("Failed to drop supplementary groups");
        }
        if (setgid(pw->pw_gid) == -1) {
            fatal_errno("Failed to drop group privileges");
        }
        if (setuid(pw->pw_uid) == -1) {
            fatal_errno("Failed to drop user privileges");
        }
    }

    if (daemonize) {
        pid_t pid = fork();
        if (pid < 0) {
            fatal_errno("Could not create daemon process");
        }
        if (pid > 0) {
            printf("Daemon started with PID: %d\n", pid);
            return EXIT_SUCCESS;
        }
        if (setsid() == -1) {
            fatal_errno("Could not detach from terminal");
        }
        if (chdir("/") != 0) {
            fatal_errno("Could not change working directory to '/'");
        }
        umask(0);
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
        openlog("wd-broker", LOG_PID | LOG_NDELAY, facility);
    }

    log_message(LOG_NOTICE, "wd-broker v%s started on socket %s", PACKAGE_VERSION, socket_path);
    log_message(LOG_INFO, "Hardware watchdog timeout: %d seconds", wd_timeout_s);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    while (running && all_ok) {
        int            maxfd = (server_sock > timer_fd ? server_sock : timer_fd) + 1;
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        int            ret = 0;
        fd_set         fds;

        FD_ZERO(&fds);
        FD_SET(server_sock, &fds);
        FD_SET(timer_fd, &fds);
        ret = select(maxfd, &fds, NULL, NULL, &timeout);
        if (ret >= 0) {
            /* If there is a select() timeout, a event on the timerfd or a new connection
             * on the server socket we also want to check the clients to detect timeouts
             * as early as possible */

            /* first process a client input to do not delay a heartbeat */
            if (FD_ISSET(server_sock, &fds)) {
                int client_sock = accept(server_sock, NULL, NULL);
                if (client_sock >= 0) {
                    handle_command(client_sock, clients);
                    close(client_sock);
                } else if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED ||
                             errno == EINTR)) {
                    log_message(LOG_ERR, "accept: %s", strerror(errno));
                    all_ok = false;
                }
            }

            /* Check if any client has missed a heartbeat */
            for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
                if (clients[i].active) {
                    unsigned int age = ms_since(&clients[i].last_ping);
                    if (age > clients[i].timeout_ms) {
                        log_message(LOG_CRIT,
                                    "Client '%s' (PID %d, clientID=%s) missed heartbeat (%u "
                                    "ms > %d ms)",
                                    clients[i].name, (int)clients[i].pid, clients[i].clientID, age,
                                    clients[i].timeout_ms);
                        all_ok = false;
                        break;
                    }
                }
            }

            /* Finally feed the watchdog if it's time and everything is ok */
            if (FD_ISSET(timer_fd, &fds)) {
                uint64_t expirations;
                ssize_t  size = read(timer_fd, &expirations, sizeof(expirations));
                if (size != sizeof(expirations)) {
                    fatal_errno("Failed to read timer file descriptor");
                }

                if (all_ok && watchdog_fd != -1) {
                    if (write(watchdog_fd, "\0", 1) != 1) {
                        fatal_error("Failed to feed watchdog: %s", strerror(errno));
                    }
                }
            }
        } else if (ret < 0 && errno != EINTR) {
            log_message(LOG_CRIT, "select: %s", strerror(errno));
            all_ok = false;
        }
    }

    if (!all_ok) {
        /* Have been kicked out of the main loop because of a client timeout.
         * Close the socket and wait for external termination, either by the
         * intentional reset if the watchdog is enabled or by the user
         * termination in test mode, makes not differnece at this point.
         * */
        log_message(LOG_EMERG, "SYSTEM RESET PENDING!");
        fflush(stdout);
        fflush(stderr);
        close(timer_fd);
        unlink(socket_path);
        while (running) {
            sleep(1);
        }
    }

    log_message(LOG_INFO, "Exiting wd-broker...");

    if (watchdog_fd != -1) {
        /* Legacy way */
        const char c = 'V';
        if (write(watchdog_fd, &c, 1) != 1) {
            log_message(LOG_ERR, "Failed to stop watchdog: %s", strerror(errno));
        }
        /* Modern way */
        int flags = WDIOS_DISABLECARD;
        if (ioctl(watchdog_fd, WDIOC_SETOPTIONS, &flags) == -1) {
            log_message(LOG_ERR, "Failed to disable watchdog: %s", strerror(errno));
        }
        close(watchdog_fd);
        watchdog_fd = -1;
    }

    closelog();
    close(timer_fd);
    unlink(socket_path);
    return 0;
}
