/*
 * wd-ctrl, a minimal command-line utility to interact with wd-broker,
 * used to query status and manage client registrations.
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
 * This utility is part of the wd-broker project, hosted on GitHub:
 *   https://github.com/fjulian79/wd-broker
 * Please feel free to file issues, open pull requests, or contribute there.
 */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"
#include "config.h"

#define BUF_SIZE  4096
#define MAX_LINES WD_MAX_CLIENTS + 10

#define arraysize(x) (sizeof(x) / sizeof((x)[0]))

void print_help(void) {
    printf("Usage: wd-ctrl [OPTIONS] status | unregister <clientID|name>\n");
    printf("\nOptions:\n");
    printf("  --help                    Show this help message and exit\n");
    printf("  --version                 Show version information and exit\n");
    printf("  --socket-path <path>      Use custom socket path (default: %s)\n",
           SOCKET_PATH_DEFAULT);
    printf("\nCommands:\n");
    printf("  status                    Show the current status of the broker and all clients\n");
    printf("  unregister <clientID|name>\n");
    printf("                            Unregister a client by its ID or name\n");
    printf("\nExamples:\n");
    printf("  wd-ctrl status\n");
    printf("  wd-ctrl unregister 4f3c0d9e8a1b4f21\n");
    printf("  wd-ctrl --socket-path /run/custom.sock status\n");
    printf("\n");
}

void print_table_header() {
    printf("\n%-18s %-6s %-20s %-13s %-8s\n", "Client ID", "PID", "Name", "Timeout (ms)",
           "pidCheck");
    printf("%.*s\n", 69, "---------------------------------------------------------------------");
}

void print_line_formatted(const char *line) {
    char     id[32], name[64];
    int      pid = 0;
    unsigned timeout = 0;
    int      checkPid = 0;

    if (sscanf(line, "%16s %d %63s %u %d", id, &pid, name, &timeout, &checkPid) == 5) {
        printf("%-18s %-6d %-20s %-13u %-8s\n", id, pid, name, timeout, checkPid ? "on" : "off");
    } else {
        fputs(line, stdout); // fallback
    }
}

int send_and_receive(const char *socket_path, const char *cmd, char *buf, size_t bufsize) {
    int                sock = 0;
    struct sockaddr_un addr = {0};
    ssize_t            len = 0;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Error: socket() failed: %s\n", strerror(errno));
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "Cannot connect to server at %s: %s\n", socket_path, strerror(errno));
        close(sock);
        return -1;
    }

    if (write(sock, cmd, strlen(cmd)) < 0) {
        if (errno == EPIPE) {
            fprintf(stderr, "Error: Broken pipe (server may have closed connection)\n");
        }
        fprintf(stderr, "Failed to send request to server: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    len = read_with_timeout(sock, buf, bufsize, false);

    close(sock);
    if (len >= 0) {
        buf[len] = '\0';
        return (int)len;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    const char          *socket_path = SOCKET_PATH_DEFAULT;
    char                 cmd[128] = {0};
    char                 buf[BUF_SIZE] = {0};
    char                *lines[MAX_LINES] = {0};
    int                  line_count = 0;
    char                *saveptr = NULL;
    char                *line = NULL;
    int                  has_client_rows = 0;
    int                  opt = 0;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"socket-path", required_argument, 0, 's'},
        {0, 0, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "hvs:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help();
                return EXIT_SUCCESS;
            case 'v':
                printf("wd-ctrl v%s\n", PACKAGE_VERSION);
                return EXIT_SUCCESS;
            case 's':
                socket_path = optarg;
                break;
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    if (geteuid() != 0) {
        fatal_error("Only root is allowed to control wd-broker.\n");
        exit(EXIT_FAILURE);
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Missing command.\n\n");
        print_help();
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    if (send_and_receive(socket_path, CMD_STATUS "\n", buf, sizeof(buf)) <= 0) {
        return EXIT_FAILURE;
    }

    line = strtok_r(buf, "\n", &saveptr);
    while (line && line_count < arraysize(lines) - 1) {
        lines[line_count++] = line;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (strcmp(argv[optind], "status") == 0) {
        for (int i = 0; i < line_count; ++i) {
            if (strchr(lines[i], '=') != NULL) {
                char key[64], val[64];
                if (sscanf(lines[i], "%63[^=]=%63s", key, val) == 2) {
                    printf("%-18s: %s\n", key, val);
                }
            } else {
                if (!has_client_rows) {
                    print_table_header();
                    has_client_rows = 1;
                }
                print_line_formatted(lines[i]);
            }
        }
    } else if (strcmp(argv[optind], "unregister") == 0 && argv[optind + 1]) {
        char *client = argv[optind + 1];
        char  id_candidate[17] = {0};
        bool  found = false;
        int   match_count = 0;

        for (int i = 0; i < line_count; ++i) {
            char     id[17], name[64];
            int      pid;
            unsigned timeout;
            if (sscanf(lines[i], "%16s %d %63s %u", id, &pid, name, &timeout) == 4) {
                if (strcmp(client, id) == 0) {
                    strncpy(id_candidate, id, sizeof(id_candidate));
                    found = true;
                    break;
                } else if (strcmp(client, name) == 0) {
                    strncpy(id_candidate, id, sizeof(id_candidate));
                    match_count++;
                    found = true;
                }
            }
        }

        if (match_count > 1) {
            fatal_error("Client name '%s' is not unique, use the clientID.\n", client);
        } else if (!found) {
            fatal_error("Unknown client (%s).\n", client);
        }

        snprintf(cmd, sizeof(cmd), "%s%s\n", CMD_UNREGISTER, id_candidate);
        if (send_and_receive(socket_path, cmd, buf, sizeof(buf)) <= 0) {
            return EXIT_FAILURE;
        }
        if (strncmp(buf, "OK", 2) == 0) {
            printf("Client '%s' unregistered successfully.\n", client);
        } else if (strncmp(buf, "ERROR", 5) == 0) {
            char *error_msg = buf + 6;
            fprintf(stderr, "Server Error: %s\n", error_msg);
            return EXIT_FAILURE;
        } else {
            fatal_error("Unexpected server response.\n");
        }
    } else {
        fprintf(stderr, "Error: Invalid arguments.\n\n");
        print_help();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}