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
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "config.h" // for PACKAGE_VERSION

#define STR_HELPER(x)            #x
#define STR(x)                   STR_HELPER(x)
#define SOCKET_PATH_DEFAULT      "/tmp/wd-broker.sock"
#define SOCKET_PATH_TEST         "/tmp/wd-broker-test.sock"
#define SOCKET_READ_TIMEOUT_MS   200
#define MAX_CLIENTS              64
#define BUF_SIZE                 128
#define LOOP_INTERVAL_DEFAULT_MS 1000
#define LOOP_INTERVAL_MIN_MS     LOOP_INTERVAL_DEFAULT_MS
#define LOOP_INTERVAL_MAX_MS     60000
#define CLIENT_NAME_LEN          64
#define CLIENT_NAME_FMT_LEN_STR  "63" // cant use (CLIENT_NAME_LEN - 1) here
#define REGISTER_SCANF_FORMAT    "%" CLIENT_NAME_FMT_LEN_STR "s %d %15s"
#define CLIENTID_LEN             9
#define CLIENTID_FMT_LEN_NUM     8 // CLIENTID_LEN - 1
#define CLIENTID_FMT_LEN_STR     STR(CLIENTID_FMT_LEN_NUM)
#define CLIENTID_SCANF_FORMAT    "%" CLIENTID_FMT_LEN_STR "s %15s"
#define CLIENT_TIMEOUT_MIN_MS    LOOP_INTERVAL_MIN_MS
#define CLIENT_TIMEOUT_MAX_MS    LOOP_INTERVAL_MAX_MS
#define CMD_REGISTER             "REGISTER "
#define CMD_PING                 "PING "
#define CMD_UNREGISTER           "UNREGISTER "

typedef struct {
    char            clientID[CLIENTID_LEN];
    char            name[CLIENT_NAME_LEN];
    uint32_t        timeout_ms;
    struct timespec last_ping;
    bool            active;
} client_t;

uint32_t      loop_interval_ms = LOOP_INTERVAL_DEFAULT_MS;
char         *socket_path = SOCKET_PATH_DEFAULT;
client_t      clients[MAX_CLIENTS];
int           watchdog_fd = -1;
volatile bool running = true;
bool          watchdog_enabled = false;
bool          test_mode = false;

void print_help(const char *progname) {
    printf("Usage: %s [--test] [--help] [--version] [--interval <ms>]\n", progname);
    printf("\nOptions:\n");
    printf("  --test             Run in test mode (no /dev/watchdog access)\n");
    printf("  --interval <ms>    Set loop interval in milliseconds (default: %d)\n",
           LOOP_INTERVAL_DEFAULT_MS);
    printf("  --help             Show this help message\n");
    printf("  --version          Show version information\n");
}

void write_or_log(int fd, const char *msg, size_t len) {
    ssize_t written = write(fd, msg, len);
    if (written < 0) {
        perror("write");
    }
}

void write_str(int fd, const char *msg) {
    write_or_log(fd, msg, strlen(msg));
}

void make_clientID(char *clientID_out) {
    static const char hex[] = "0123456789abcdef";
    for (uint8_t i = 0; i < CLIENTID_LEN - 1; ++i) {
        clientID_out[i] = hex[rand() % 16];
    }
    clientID_out[CLIENTID_LEN - 1] = '\0';
}

bool parse_clientID(const char *buf, const char *cmd_prefix, int sock, char *out_id) {
    char clientID_raw[CLIENTID_LEN];
    char extra[16];
    int  n = sscanf(buf + strlen(cmd_prefix), CLIENTID_SCANF_FORMAT, clientID_raw, extra);

    /* Try to match exactly one argument, and reject trailing garbage */
    if (n != 1) {
        write_str(sock, "ERROR invalid syntax\n");
        return false;
    }

    /* Check if clientID is a valid hex string */
    size_t id_len = strlen(clientID_raw);
    if (id_len != CLIENTID_FMT_LEN_NUM) {
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

void get_now(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

uint32_t ms_since(struct timespec *then) {
    struct timespec now;
    get_now(&now);
    return (now.tv_sec - then->tv_sec) * 1000 + (now.tv_nsec - then->tv_nsec) / 1000000;
}

void signal_handler(int sig) {
    running = false;
}

void handle_command(int client_sock) {
    char           buf[BUF_SIZE];
    char           clientID[CLIENTID_LEN];
    ssize_t        len = 0;
    struct timeval recv_timeout = {
        .tv_sec = 0,
        .tv_usec = SOCKET_READ_TIMEOUT_MS * 100,
    };

    /* Mandatory to avoid blocking in read when the a cleint does not send any inputs */
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    len = read(client_sock, buf, sizeof(buf) - 1);
    if (len <= 0) {
        write_str(client_sock, "ERROR no input\n");
        return;
    }
    buf[len] = '\0';

    if (strncmp(buf, CMD_REGISTER, strlen(CMD_REGISTER)) == 0) {
        char name[CLIENT_NAME_LEN];
        int  tmp_timeout = 0;
        char extra[16]; // catches unexpected extra input
        int  n = 0;

        /* Try to match exactly two arguments, and reject trailing garbage */
        n = sscanf(buf + strlen(CMD_REGISTER), REGISTER_SCANF_FORMAT, name, &tmp_timeout, extra);
        if (n != 2) {
            write_str(client_sock, "ERROR invalid REGISTER syntax\n");
            return;
        }

        /* Validate timeout range */
        if (tmp_timeout < CLIENT_TIMEOUT_MIN_MS || tmp_timeout > CLIENT_TIMEOUT_MAX_MS) {
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
        for (uint8_t i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i].active) {
                clients[i].active = true;
                strncpy(clients[i].name, name, sizeof(clients[i].name) - 1);
                clients[i].timeout_ms = (uint32_t)tmp_timeout;
                get_now(&clients[i].last_ping);
                make_clientID(clients[i].clientID);
                printf("INFO: client '%s' registered with timeout %d ms (clientID=%s)\n",
                       clients[i].name, clients[i].timeout_ms, clients[i].clientID);
                dprintf(client_sock, "OK %s\n", clients[i].clientID);
                return;
            }
        }

        /* If we reach this point, all client slots are taken */
        write_str(client_sock, "ERROR too many clients\n");

    } else if (strncmp(buf, CMD_PING, strlen(CMD_PING)) == 0) {
        char clientID[CLIENTID_LEN];

        if (!parse_clientID(buf, CMD_PING, client_sock, clientID)) {
            return;
        }

        /* Check if clientID is registered and confirm the PING if so */
        for (uint8_t i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active &&
                strncmp(clients[i].clientID, clientID, CLIENTID_FMT_LEN_NUM) == 0) {
                get_now(&clients[i].last_ping);
                write_str(client_sock, "OK\n");
                return;
            }
        }

        /* If we reach this point, the clientID is not registered */
        write_str(client_sock, "ERROR unknown clientID\n");

    } else if (strncmp(buf, CMD_UNREGISTER, strlen(CMD_UNREGISTER)) == 0) {
        char clientID[CLIENTID_LEN];

        if (!parse_clientID(buf, CMD_UNREGISTER, client_sock, clientID)) {
            return;
        }

        /* Check if clientID is registered and unregister it if so */
        for (uint8_t i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active && strncmp(clients[i].clientID, clientID, CLIENTID_LEN) == 0) {
                printf("INFO: client '%s' unregistered (clientID=%s)\n", clients[i].name,
                       clients[i].clientID);
                clients[i].active = false;
                write_str(client_sock, "OK\n");
                return;
            }
        }

        /* If we reach this point, the clientID is not registered */
        write_str(client_sock, "ERROR unknown clientID\n");

    } else {
        write_str(client_sock, "ERROR unknown command\n");
    }
}

int main(int argc, char *argv[]) {
    bool all_ok = true;

    int                server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};

    int               timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec timer_spec = {
        .it_interval = {.tv_sec = loop_interval_ms / 1000,
                        .tv_nsec = (loop_interval_ms % 1000) * 1000000},
        .it_value = {.tv_sec = loop_interval_ms / 1000,
                     .tv_nsec = (loop_interval_ms % 1000) * 1000000},
    };

    static struct option long_options[] = {
        {"test", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"interval", required_argument, 0, 'i'},
        {0, 0, 0, 0},
    };
    int opt;

    while ((opt = getopt_long(argc, argv, "thvi:", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                test_mode = true;
                break;
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'v':
                printf("wd-broker v%s\n", PACKAGE_VERSION);
                return 0;
            case 'i':
                loop_interval_ms = atoi(optarg);
                break;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    if (loop_interval_ms < LOOP_INTERVAL_MIN_MS || loop_interval_ms > LOOP_INTERVAL_MAX_MS) {
        fprintf(stderr, "Error: Loop interval must be between %d and %d ms.\n",
                LOOP_INTERVAL_MIN_MS, LOOP_INTERVAL_MAX_MS);
        return (EXIT_FAILURE);
    }

    if (!test_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid > 0) {
            return 0; // parent exits
        }
        setsid();
        chdir("/");
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
    } else {
        socket_path = SOCKET_PATH_TEST;
        printf("Running in test mode. /dev/watchdog will not be used.\n");
        printf("Using socket path: %s\n", socket_path);
    }

    srand(time(NULL));
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    unlink(socket_path);
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);

    chmod(socket_path, 0666);

    if (!test_mode) {
        watchdog_fd = open("/dev/watchdog", O_WRONLY);
        if (watchdog_fd >= 0) {
            watchdog_enabled = true;
        } else {
            perror("Failed to open /dev/watchdog");
            exit(EXIT_FAILURE);
        }
    }

    timerfd_settime(timer_fd, 0, &timer_spec, NULL);

    while (running && all_ok) {
        int            maxfd = (server_sock > timer_fd ? server_sock : timer_fd) + 1;
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 500000};
        int            ret = 0;
        fd_set         fds;

        FD_ZERO(&fds);
        FD_SET(server_sock, &fds);
        FD_SET(timer_fd, &fds);

        ret = select(maxfd, &fds, NULL, NULL, &timeout);
        if (ret > 0) {
            if (FD_ISSET(server_sock, &fds)) {
                int client_sock = accept(server_sock, NULL, NULL);
                if (client_sock >= 0) {
                    handle_command(client_sock);
                    close(client_sock);
                }
            }
            if (FD_ISSET(timer_fd, &fds)) {
                uint64_t expirations;
                read(timer_fd, &expirations, sizeof(expirations));

                for (uint8_t i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i].active) {
                        uint32_t age = ms_since(&clients[i].last_ping);
                        if (age > clients[i].timeout_ms) {
                            printf("WARNING: client '%s' (clientID=%s) missed heartbeat (%u ms > "
                                   "%d ms)\n",
                                   clients[i].name, clients[i].clientID, age,
                                   clients[i].timeout_ms);
                            all_ok = false;
                            break;
                        }
                    }
                }
                if (watchdog_enabled && all_ok) {
                    // Feed the watchdog
                    write(watchdog_fd, "V", 1);
                }
            }
        }
    }

    if (!all_ok) {
        /* Have been kicked out of the main loop because of a client timeout.
         * Close the socket and wait for external termination, ether by the
         * intentional reset if the watchdog is enabled or by the user
         * termination in test mode, makes not differnece at this point.
         * */
        printf("ERROR: CLIENT HEARTBEAT TIMEOUT OCCURED, SYSTEM RESET PENDING!\n");
        fflush(stdout);
        close(timer_fd);
        unlink(socket_path);
        while (running) {
            sleep(1);
        }
    }

    if (watchdog_enabled) {
        write(watchdog_fd, "V", 1);
        close(watchdog_fd);
    }
    close(timer_fd);
    unlink(socket_path);
    return 0;
}
