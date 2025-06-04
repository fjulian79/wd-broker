/*
 * wd-broker – Robust watchdog supervisor daemon for embedded Linux systems.
 *
 * Designed to serve as the authoritative watchdog interface on systems with
 * multiple cooperating processes. It coordinates watchdog feeding centrally
 * and ensures each registered client meets its heartbeat obligations.
 *
 *     Copyright 2025 Julian Friedrich
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is provided on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Source repository: https://github.com/fjulian79/wd-broker
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
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
#include "wd-client.h"

#define STR_HELPER(x)              #x
#define STR(x)                     STR_HELPER(x)
#define DEFAULT_CONFIG_PATH        SYSCONFDIR "/wd-broker.conf"
#define WD_HW_TIMEOUT_DEFAULT_S    10
#define WD_HW_TIMEOUT_MIN_S        WD_HW_TIMEOUT_DEFAULT_S
#define WD_HW_TIMEOUT_MAX_S        60
#define WD_CLIENT_NAME_FMT_LEN_STR "63" // cant use (CLIENT_NAME_LEN - 1) here
#define WD_REGISTER_SCANF_FORMAT   "%" WD_CLIENT_NAME_FMT_LEN_STR "s %d %15s %15s"
#define WD_CLIENTID_FMT_LEN        16 // Must be a number cant use CLIENTID_LEN - 1
#define WD_CLIENTID_FMT_STR        STR(WD_CLIENTID_FMT_LEN)
#define WD_CLIENTID_SCANF_FORMAT   "%" WD_CLIENTID_FMT_STR "s %15s"
#define WD_CLIENT_IDENTIFIED       0
#define WD_CLIENT_PID_MISMATCH     -1
#define WD_CLIENT_NOT_FOUND        -2

typedef struct {
    int         flag;
    const char *name;
} BootFlag;

typedef struct {
    char            clientID[WD_CLIENTID_LEN];
    char            name[WD_CLIENT_NAME_LEN];
    pid_t           pid;
    unsigned int    timeout_ms;
    struct timespec last_ping;
    bool            active;
    bool            anounced;
    bool            checkPID;
} client_t;

typedef struct {
    char socket_path[WD_CLIENT_SOCKPATH_LEN];
    int  wd_timeout_s;
    bool strict_clients;
    bool unique_clients;
} wd_config_t;

static const BootFlag bootflags[] = {
    { WDIOF_OVERHEAT,  " OVERHEAT" },
    { WDIOF_FANFAULT,  " FANFAULT" },
    { WDIOF_EXTERN1,   " EXTERN1" },
    { WDIOF_EXTERN2,   " EXTERN2" },
    { WDIOF_POWERUNDER," POWERUNDER" },
    { WDIOF_CARDRESET, " CARDRESET" },
    { WDIOF_POWEROVER, " POWEROVER" },
};

wd_config_t   config;
volatile bool running = true;
bool          daemonize = false;

void print_help(void) {
    printf("Usage: wd-broker [OPTIONS]\n");
    printf("\nOptions:\n");
    printf("  --help                    Show this help message and exit\n");
    printf("  --version                 Show version information and exit\n");
    printf("  --daemonize               Run as a daemon (default: false)\n");
    printf("  --config-file <file>      Load configuration from file (default: %s)\n",
           DEFAULT_CONFIG_PATH);
    printf("  --service-user <user>     Set the service user to drop privileges to (default: %s)\n",
           SERVICE_USER_DEFAULT);
    printf("                            ATTENTION: Must not be 'root'\n");
    printf("  --syslog-facility <name>  Set syslog facility when running as a daemon\n");
    printf("                            Supported values: LOG_DAEMON (default), LOG_USER,\n");
    printf("                            LOG_LOCAL0 through LOG_LOCAL7\n");
    printf("  --no-watchdog             Disable hardware watchdog (test mode, default: false)\n");
    printf("\nExamples:\n");
    printf("  wd-broker --no-watchdog --config-file /tmp/test-config\n");
    printf("  wd-broker --daemonize\n");
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

static char *trim(char *str) {
    if (!str) {
        return str;
    }
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    return str;
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

void make_clientID(char *clientID_out) {
    static const char hex[] = "0123456789abcdef";
    uint8_t           raw[WD_CLIENTID_LEN / 2];
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
        for (size_t i = 0; i < sizeof(raw); ++i) {
            raw[i] = random() & 0xFF;
        }
    }

    /* Convert the raw bytes to a hex string */
    for (size_t i = 0; i < sizeof(raw); ++i) {
        clientID_out[i * 2] = hex[(raw[i] >> 4) & 0x0F];
        clientID_out[i * 2 + 1] = hex[raw[i] & 0x0F];
    }

    clientID_out[WD_CLIENTID_LEN - 1] = '\0';
}

void get_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

void parse_config(const char *filename, client_t *clients, size_t max_clients) {
    FILE *f = fopen(filename, "r");
    char  line[256] = {0};
    char  key[64] = {0};
    char  val[128] = {0};
    char  name[WD_CLIENT_NAME_LEN] = {0};
    int   client_idx = -1;
    int   line_num = 0;

    if (f == NULL) {
        fatal_error("Failed to open %s", filename);
    }

    /* Reset the client array, it will be filled with the announced clients
     * given by the config file starting at index 0. The rest of the array
     * will be used for clients that register unexpected. */
    memset(clients, 0, sizeof(client_t) * max_clients);

    /* Initialize config with default values to be save */
    memset(&config, 0, sizeof(config));
    strncpy(config.socket_path, SOCKET_PATH_DEFAULT, sizeof(config.socket_path));
    config.wd_timeout_s = WD_HW_TIMEOUT_DEFAULT_S;
    config.strict_clients = false;
    config.unique_clients = false;

    /* Read the config file line by line. Be strict about the format. Abbort in case of any error to
     * raise awareness. */
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);
        line_num++;

        /* Skip empty lines and comments */
        if (*trimmed == '#' || *trimmed == '\0') {
            continue;
        }

        /* Check for key-value pairs */
        if (sscanf(trimmed, "%63[^=]=%127[^\n]", key, val) == 2) {
            char *t_key = trim(key);
            char *t_val = trim(val);

            if (strcmp(t_key, "socket_path") == 0) {
                strncpy(config.socket_path, t_val, sizeof(config.socket_path) - 1);
            } else if (strcmp(t_key, "wd_timeout_s") == 0) {
                config.wd_timeout_s = atoi(t_val);
            } else if (strcmp(t_key, "strict_clients") == 0) {
                if (strcmp(t_val, "true") == 0) {
                    config.strict_clients = true;
                } else if (strcmp(t_val, "false") == 0) {
                    config.strict_clients = false;
                } else {
                    fclose(f);
                    fatal_error("Invalid value for strict_clients in line %d", line_num);
                }
            } else if (strcmp(t_key, "unique_clients") == 0) {
                if (strcmp(t_val, "true") == 0) {
                    config.unique_clients = true;
                } else if (strcmp(t_val, "false") == 0) {
                    config.unique_clients = false;
                } else {
                    fclose(f);
                    fatal_error("Invalid value for unique_clients in line %d", line_num);
                }
            } else if (strcmp(t_key, "timeout_ms") == 0) {
                if (client_idx == -1) {
                    fclose(f);
                    fatal_error("Timeout must be set in a client section");
                }
                clients[client_idx].timeout_ms = atoi(t_val);
                if (clients[client_idx].timeout_ms < WD_CLIENT_TIMEOUT_MIN_MS ||
                    clients[client_idx].timeout_ms > WD_CLIENT_TIMEOUT_MAX_MS) {
                    fclose(f);
                    fatal_error("Timeout must be between %d and %d ms", WD_CLIENT_TIMEOUT_MIN_MS,
                                WD_CLIENT_TIMEOUT_MAX_MS);
                }
            } else {
                /* Catch all unknown keys */
                fclose(f);
                fatal_error("Unknown config key '%s' in line %d", t_key, line_num);
            }
            continue;
        }

        /* Check for client section */
        if (sscanf(trimmed, "[client %63[^]]", name) == 1) {
            client_idx++;
            if (client_idx >= (int)max_clients) {
                fclose(f);
                fatal_error("Too many clients defined in %s", filename);
            }
            if (strlen(trimmed) - strlen(name) > strlen("[client ]")) {
                fclose(f);
                fatal_error("Invalid client name used in line %d", line_num);
            }
            strncpy(clients[client_idx].name, name, sizeof(clients[client_idx].name));
            /* Assume no further specs and set defaults */
            make_clientID(clients[client_idx].clientID);
            clients[client_idx].timeout_ms = 5 * 60 * 1000; // 5 minutes
            clients[client_idx].anounced = true;
            clients[client_idx].active = false;
            get_now(&clients[client_idx].last_ping);
            continue;
        }

        /* If this point is reached, the line is not valid, assume a broken config file */
        fclose(f);
        fatal_error("Invalid config in line: %d: %s\n", line_num, trimmed);
    }

    if (config.strict_clients && client_idx == -1) {
        fclose(f);
        fatal_error("No client section found in %s", filename);
    }

    /* Verify that all announced clients have unique names if unique_clients is enabled.
     *
     * This uses a naive O(n²) algorithm. However, the maximum number of clients is limited
     * (default: 64), and usually not all of them are announced clients. The check is performed only
     * once at program startup when reading the configuration file. Even on slower embedded
     * hardware, the runtime impact is considered negligible (well below 5 ms in the worst case).
     * For this reason, no more complex data structure (e.g., a hash set) is used, in order to keep
     * the code simple, portable, and easy to maintain. */
    if (config.unique_clients) {
        for (size_t i = 0; i < max_clients; ++i) {
            if (clients[i].anounced) {
                for (size_t j = i + 1; j < max_clients; ++j) {
                    if (clients[j].anounced && strcmp(clients[i].name, clients[j].name) == 0) {
                        fclose(f);
                        fatal_error("Duplicate client name '%s' in config file", clients[i].name);
                    }
                }
            }
        }
    }

    fclose(f);
}

void log_bootstatus(int bootstatus) {
    char flags[128] = {0};
    for (size_t i = 0; i < sizeof(bootflags) / sizeof(bootflags[0]); ++i) {
        if (bootstatus & bootflags[i].flag) {
            strncat(flags, bootflags[i].name, sizeof(flags) - strlen(flags) - 1);
        }
    }

    if (strlen(flags) == 0) {
        strncat(flags, " none", sizeof(flags) - 1);
    }

    log_message(LOG_INFO, "Boot status 0x%08x:%s", bootstatus, flags);
}

void write_str(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    if (vdprintf(fd, fmt, ap) < 0) {
        log_message(LOG_ERR, "vdprintf: %s", strerror(errno));
    }

    va_end(ap);
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

/* Find a slot for registration of a cient with the given name and pid.
 * Implement the following logic:
 *
 * strict_client = false, unique_client = false
 * Find a slot where announced is true, active is false and the name matches.
 * OR a slot where announced and active are false.
 * If none is found, return null.
 *
 * strict_client = true,  unique_client = false
 * Find a slot where announced is true, active is false and the name matches.
 * If none is found, return null.
 *
 * strict_client = false, unique_client = true
 * Find a slot where announced is true, active is false and the name matches.
 * OR a slot where announced and active are false.
 * If there is any slot where active is true and the name matches, the client can't register.
 * If none is found, return null.
 *
 * strict_client = true,  unique_client = true
 * Find a slot where announced is true, active is false and the name matches.
 * If there is any slot where active is true and the name matches, the client can't register.
 * If none is found, return null.
 */
client_t *get_clientInstance_by_name(client_t *clients, const char *name, pid_t pid) {
    client_t *free_slot = NULL;
    client_t *announced_slot = NULL;

    // unique_clients: Check if any active client with the same name exists
    if (config.unique_clients) {
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (clients[i].active && strcmp(clients[i].name, name) == 0) {
                log_message(LOG_NOTICE,
                            "Rejected PID%d with name '%s', name already in use by PID%d", (int)pid,
                            name, (int)clients[i].pid);
                return NULL;
            }
        }
    }

    for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
        // Find announced, inactive slot with matching name
        if (clients[i].anounced && !clients[i].active && strcmp(clients[i].name, name) == 0) {
            announced_slot = &clients[i];
            break; // Always prefer announced slot if available
        }
        // Find first free slot (not announced, not active)
        if (!clients[i].anounced && !clients[i].active && free_slot == NULL) {
            free_slot = &clients[i];
        }
    }

    if (config.strict_clients) {
        if (!announced_slot) {
            log_message(LOG_NOTICE, "Rejected PID%d with name '%s', not announced", (int)pid, name);
        }
        return announced_slot;
    } else {
        if (announced_slot) {
            return announced_slot;
        } else {
            if (!free_slot) {
                log_message(LOG_NOTICE, "Rejected PID%d with name '%s', no free slot available",
                            (int)pid, name);
            }
            return free_slot;
        }
    }
}

int32_t get_clientInstance_by_clientID(client_t *clients, const char *clientID, pid_t pid,
                                       client_t **client) {
    for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
        if ((clients[i].active || clients[i].anounced) &&
            strncmp(clients[i].clientID, clientID, WD_CLIENTID_FMT_LEN) == 0) {
            *client = &clients[i];
            if (clients[i].checkPID == false || clients[i].pid == pid) {
                return WD_CLIENT_IDENTIFIED;
            } else {
                return WD_CLIENT_PID_MISMATCH;
            }
        }
    }
    return WD_CLIENT_NOT_FOUND;
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

    len = read_with_timeout(client_sock, buf, sizeof(buf));
    if (len <= 0) {
        write_str(client_sock, "ERROR no input\n");
        return;
    }

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

        client_t *pClient = get_clientInstance_by_name(clients, name, creds.pid);
        if (pClient != NULL) {
            pClient->active = true;
            strncpy(pClient->name, name, sizeof(pClient->name));
            pClient->pid = creds.pid;
            pClient->checkPID = checkPID;
            pClient->timeout_ms = tmp_timeout;
            get_now(&pClient->last_ping);
            if (!pClient->anounced) {
                /* announced clients get their clientID while parsing the config file */
                make_clientID(pClient->clientID);
            }
            log_message(
                LOG_INFO, "%s '%s' (PID %d) registered (timeout %d ms, pidCheck %s, clientID %s)",
                pClient->anounced ? "Announced client" : "Client", pClient->name, (int)pClient->pid,
                pClient->timeout_ms, checkPID ? "enabled" : "disabled", pClient->clientID);
            write_str(client_sock, "OK %s\n", pClient->clientID);
            pClient->anounced = false;
            return;
        }

        /* If we reach this point, the client can't register, shall we tell him why? */
        write_str(client_sock, "ERROR rejected\n");
        return;

    } else if (strncmp(buf, CMD_PING, strlen(CMD_PING)) == 0) {
        char clientID[WD_CLIENTID_LEN] = {0};

        if (!parse_clientID(buf, CMD_PING, client_sock, clientID)) {
            return;
        }

        ret = get_clientInstance_by_clientID(clients, clientID, creds.pid, &pClient);
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

        ret = get_clientInstance_by_clientID(clients, clientID, creds.pid, &pClient);
        switch (ret) {
            case WD_CLIENT_IDENTIFIED:
                if (config.strict_clients && creds.uid != 0) {
                    log_message(LOG_WARNING,
                                "UNREGISTER rejected for client '%s' (PID %d) with clientID %s",
                                pClient->name, (int)pClient->pid, pClient->clientID);
                    write_str(client_sock, "ERROR rejected\n");
                    return;
                } else {
                    log_message(LOG_INFO, "Client '%s' (PID %d) unregistered (clientID=%s)",
                                pClient->name, (int)pClient->pid, pClient->clientID);
                    pClient->active = false;
                    pClient->anounced = false;
                    write_str(client_sock, "OK\n");
                    return;
                }
            case WD_CLIENT_PID_MISMATCH:
                if (creds.uid == 0) {
                    pClient->active = false;
                    pClient->anounced = false;
                    write_str(client_sock, "OK\n");
                    log_message(LOG_INFO, "Client '%s' (PID %d) unregistered by root (clientID=%s)",
                                pClient->name, (int)pClient->pid, pClient->clientID);
                    return;
                } else {
                    write_str(client_sock, "ERROR no permission\n");
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
        char   extra[32];
        char   msg[4096] = {0};
        size_t offset = 0;
        size_t active_clients = 0;

        if (sscanf(buf + strlen(CMD_STATUS), " %31s", extra) == 1) {
            write_str(client_sock, "ERROR invalid syntax\n");
            return;
        }

        if (creds.pid != 0 && creds.uid != 0) {
            write_str(client_sock, "ERROR no permission\n");
            log_message(LOG_WARNING, "Client with PID %d tried to list clients", (int)creds.pid);
            return;
        }

        offset +=
            snprintf(msg + offset, sizeof(msg) - offset, "daemon_version=%s\n", PACKAGE_VERSION);
        offset += snprintf(msg + offset, sizeof(msg) - offset, "protocol_version=%s\n",
                           SOCKET_PROT_VERSION);
        offset +=
            snprintf(msg + offset, sizeof(msg) - offset, "wd_timeout_s=%d\n", config.wd_timeout_s);
        offset += snprintf(msg + offset, sizeof(msg) - offset, "strict_clients=%s\n",
                           config.strict_clients ? "true" : "false");
        offset += snprintf(msg + offset, sizeof(msg) - offset, "unique_clients=%s\n",
                           config.unique_clients ? "true" : "false");
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (clients[i].active) {
                active_clients++;
            }
        }
        offset +=
            snprintf(msg + offset, sizeof(msg) - offset, "active_clients=%zu\n", active_clients);
        for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
            if (clients[i].active || clients[i].anounced) {
                offset += snprintf(msg + offset, sizeof(msg) - offset, "%s %d %s %u %d\n",
                                   clients[i].clientID, (int)clients[i].pid, clients[i].name,
                                   clients[i].timeout_ms, clients[i].checkPID ? 1 : 0);
            }
        }

        if (write(client_sock, msg, offset) < 0) {
            log_message(LOG_ERR, "Failed to send status message: %s", strerror(errno));
        }
    } else if (strncmp(buf, CMD_VERSION, strlen(CMD_VERSION)) == 0) {
        char   extra[32];
        char   msg[128] = {0};
        size_t offset = 0;
        if (sscanf(buf + strlen(CMD_VERSION), " %31s", extra) == 1) {
            write_str(client_sock, "ERROR invalid syntax\n");
            return;
        }

        offset +=
            snprintf(msg + offset, sizeof(msg) - offset, "wd-broker_version=%s\n", PACKAGE_VERSION);
        offset += snprintf(msg + offset, sizeof(msg) - offset, "protocol_version=%s\n",
                           SOCKET_PROT_VERSION);
        if (write(client_sock, msg, offset) < 0) {
            log_message(LOG_ERR, "Failed to send status message: %s", strerror(errno));
        }
    } else {
        write_str(client_sock, "ERROR unknown command\n");
    }
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

int main(int argc, char *argv[]) {
    client_t           clients[WD_MAX_CLIENTS] = {0};
    int                watchdog_fd = -1;
    bool               watchdog_enabled = true;
    bool               all_ok = true;
    int                server_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un addr = {0};
    struct stat        st;
    int                timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec  timer_spec = {0};
    int                syslog_facility = LOG_DAEMON;
    char              *service_user = SERVICE_USER_DEFAULT;
    bool               started_as_root = geteuid() == 0 ? true : false;
    char              *config_file = DEFAULT_CONFIG_PATH;
    struct passwd     *pw = NULL;

    static struct option long_options[] = {
        {"daemonize", no_argument, 0, 'd'},
        {"config-file", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"no-watchdog", no_argument, 0, 'n'},
        {"service-user", required_argument, 0, 'u'},
        {"syslog-facility", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'd':
                daemonize = true;
                break;
            case 'h':
                print_help();
                return EXIT_SUCCESS;
            case 'n':
                watchdog_enabled = false;
                break;
            case 's':
                syslog_facility = parse_syslog_facility(optarg);
                break;
            case 'u':
                service_user = optarg;
                break;
            case 'v':
                printf("wd-broker v%s\n", PACKAGE_VERSION);
                return 0;
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    /* Check the given service user and the permissions of the config file */
    pw = getpwnam(service_user);
    if (strlen(service_user) == 0 || strlen(service_user) > LOGIN_NAME_MAX) {
        fatal_error("Invalid service user name '%s'.\n", service_user);
    }
    if (!pw) {
        fatal_error("Service user '%s' does not exist.\n", service_user);
    }
    if (pw->pw_uid == 0) {
        fatal_error("Refusing to drop privileges to root, use a different service user.\n");
    }
    if (lstat(config_file, &st) != 0) {
        fatal_error("Cannot stat config file '%s': %s", config_file, strerror(errno));
    }
    if (!S_ISREG(st.st_mode)) {
        fatal_error("Config file '%s' is not a regular file", config_file);
    }
    if (S_ISLNK(st.st_mode)) {
        fatal_error("Config file '%s' must not be a symlink", config_file);
    }
    if (st.st_uid != pw->pw_uid) {
        fatal_error("Config file '%s' must be owned by '%s' (uid %d)\n", config_file, service_user,
                    pw->pw_uid);
    }
    if ((st.st_mode & 022) != 0) {
        fatal_error("Config file '%s' must not be writable by group or others", config_file);
    }

    parse_config(config_file, clients, WD_MAX_CLIENTS);

    if (config.wd_timeout_s < WD_HW_TIMEOUT_MIN_S || config.wd_timeout_s > WD_HW_TIMEOUT_MAX_S) {
        fatal_error("Invalid watchdog timeout: must be between %d and %d seconds.\n",
                    WD_HW_TIMEOUT_MIN_S, WD_HW_TIMEOUT_MAX_S);
    }

    /* We want to tick faster than the watchdog timeout, so we set the timer to half the timeout to 
     * prevent unintended watchdog resets in case of high CPU load or other performance issues.
     * Using watchdog timeout /2 seems to be quite common in practice.
     */
    timer_spec.it_interval.tv_sec = config.wd_timeout_s / 2;
    timer_spec.it_value.tv_sec = timer_spec.it_interval.tv_sec;
    if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) == -1) {
        fatal_errno("timerfd_settime failed");
    }

    /* Do not trust the given socket path, check if it is a socket and remove it if it is */
    if (lstat(config.socket_path, &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            fatal_error("Cannot use '%s'. File exists but is not a socket.\n"
                        "Hint: Remove the file or choose a different path.",
                        config.socket_path);
        }

        int probe_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (probe_fd < 0) {
            fatal_errno("Could not create socket to test for active broker");
        }

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, config.socket_path, sizeof(addr.sun_path));
        if (connect(probe_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            close(probe_fd);
            fatal_error("Active wd-broker instance detected at '%s'.\n", config.socket_path);
        }

        if (errno != ECONNREFUSED) {
            close(probe_fd);
            fatal_errno("Unexpected error when probing existing socket");
        }

        close(probe_fd);

        if (unlink(config.socket_path) == 0) {
            printf("Removed stale socket at '%s'\n", config.socket_path);
        } else {
            fatal_errno("Could not remove stale socket file");
        }
    }

    /* socket path is fine, create it */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, config.socket_path, sizeof(addr.sun_path));
    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno == EACCES) {
            fprintf(stderr, "Error: Permission denied to bind to socket path '%s'\n",
                    config.socket_path);
            if (!started_as_root) {
                fprintf(stderr, "Please run as root or use a different socket path\n");
            }
        } else {
            fatal_error("Failed to bind to socket path '%s'\n", config.socket_path);
        }
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock, 5) == -1) {
        fatal_errno("Could not listen on the server socket");
    }

    /* Set socket permissions to the service user and group */
    if (chmod(config.socket_path, 0660) == -1) {
        fatal_errno("Could not set permissions on socket file");
    }
    if (started_as_root && chown(config.socket_path, pw->pw_uid, pw->pw_gid) == -1) {
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
        if (ioctl(watchdog_fd, WDIOC_SETTIMEOUT, &config.wd_timeout_s) == -1) {
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
        openlog("wd-broker", LOG_PID | LOG_NDELAY, syslog_facility);
    }

    log_message(LOG_NOTICE, "wd-broker v%s started on socket %s", PACKAGE_VERSION,
                config.socket_path);
    if (watchdog_fd != -1) {
        int bootstatus = 0;
        log_message(LOG_INFO, "Opened hardware watchdog device /dev/watchdog");
        log_message(LOG_INFO, "Hardware watchdog timeout: %d seconds", config.wd_timeout_s);
        if (ioctl(watchdog_fd, WDIOC_GETBOOTSTATUS, &bootstatus) == -1) {
            log_message(LOG_ERR, "Failed to get boot status: %s", strerror(errno));
        } else {
            log_bootstatus(bootstatus);
        }
    } else {
        log_message(LOG_INFO, "Running in test mode, no hardware watchdog used");
    }
    for (size_t i = 0; i < WD_MAX_CLIENTS; ++i) {
        if (clients[i].anounced) {
            log_message(LOG_INFO, "Client '%s' announced, expected within %d ms", clients[i].name,
                        clients[i].timeout_ms);
        } else {
            break;
        }
    }

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
                /* Check if the client is active or has been anounced but not yet registered */
                if (clients[i].active || clients[i].anounced) {
                    unsigned int age = ms_since(&clients[i].last_ping);
                    if (age > clients[i].timeout_ms) {
                        if (clients[i].active) {
                            log_message(LOG_ALERT,
                                        "Client '%s' (PID %d, clientID=%s) missed heartbeat (%u ms "
                                        "> %d ms)",
                                        clients[i].name, (int)clients[i].pid, clients[i].clientID,
                                        age, clients[i].timeout_ms);
                        } else {
                            log_message(LOG_WARNING, "Client '%s' not registered within %u ms",
                                        clients[i].name, clients[i].timeout_ms);
                        }
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
        unlink(config.socket_path);
        while (running) {
            sync();
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
    unlink(config.socket_path);
    return 0;
}
