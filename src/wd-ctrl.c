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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "config.h"

#define BUF_SIZE  4096
#define MAX_LINES MAX_CLIENTS + 10

#define arraysize(x) (sizeof(x) / sizeof((x)[0]))

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_help(const char *progname) {
    printf("Usage: %s [OPTIONS] status | unregister <clientID|name>\n", progname);
    printf("\nOptions:\n");
    printf("  --socket-path <path>  Use custom socket path (default: %s)\n", SOCKET_PATH_DEFAULT);
    printf("  --help                Show this help message\n");
    printf("  --version             Show version information\n");
}

void print_table_header() {
    printf("\n%-18s %-6s %-20s %-12s\n", "Client ID", "PID", "Name", "Timeout (ms)");
    printf("%.*s\n", 60, "------------------------------------------------------------");
}

void print_line_formatted(const char *line) {
    char     id[32], name[64];
    int      pid;
    unsigned timeout;

    if (sscanf(line, "%16s %d %63s %u", id, &pid, name, &timeout) == 4) {
        printf("%-18s %-6d %-20s %-12u\n", id, pid, name, timeout);
    } else {
        fputs(line, stdout); // fallback
    }
}

int send_and_receive(const char *socket_path, const char *cmd, char *buf, size_t bufsize) {
    int                sock = 0;
    struct sockaddr_un addr = {0};
    fd_set             fds;
    struct timeval     tv = {0};
    int                ret = 0;
    ssize_t            len, total = 0;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        die("socket");
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        die("connect");
    }

    if (write(sock, cmd, strlen(cmd)) < 0) {
        if (errno == EPIPE) {
            fprintf(stderr, "Error: Broken pipe (server may have closed connection)\n");
        }
        perror("write");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec = 1;
    ret = select(sock + 1, &fds, NULL, NULL, &tv);
    if (ret == -1) {
        perror("select");
        exit(EXIT_FAILURE);
    } else if (ret == 0) {
        return -1;
    }

    while ((len = read(sock, buf + total, bufsize - 1 - total)) > 0) {
        total += len;
    }

    close(sock);
    if (total >= 0) {
        buf[total] = '\0';
    }
    return (int)total;
}

int main(int argc, char *argv[]) {
    const char          *socket_path = SOCKET_PATH_DEFAULT;
    char                 cmd[128] = {0};
    char                 buf[BUF_SIZE] = {0};
    ssize_t              len = 0;
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
                print_help(argv[0]);
                return EXIT_SUCCESS;
            case 'v':
                printf("wd-ctrl v%s\n", PACKAGE_VERSION);
                return EXIT_SUCCESS;
            case 's':
                socket_path = optarg;
                break;
            default:
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (geteuid() != 0) {
        fprintf(stderr, "Error, only root is allowed to control wd-broker.\n");
        exit(EXIT_FAILURE);
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing command.\n\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    len = send_and_receive(socket_path, "STATUS", buf, sizeof(buf));
    if (len <= 0) {
        fprintf(stderr, "Error: No response from server.\n");
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
            fprintf(stderr, "Error: client name '%s' is not unique.\n", client);
            return EXIT_FAILURE;
        } else if (!found) {
            fprintf(stderr, "Error: no client with ID or name '%s' found.\n", client);
            return EXIT_FAILURE;
        }
        printf("%s\n", id_candidate);
        snprintf(cmd, sizeof(cmd), "UNREGISTER %s\n", id_candidate);
        len = send_and_receive(socket_path, cmd, buf, sizeof(buf));
        if (len <= 0) {
            fprintf(stderr, "Error: No response from server.\n");
            return EXIT_FAILURE;
        }
        if (strncmp(buf, "OK", 2) == 0) {
            printf("Client '%s' unregistered successfully.\n", client);
        } else if (strncmp(buf, "ERROR", 5) == 0) {
            char *error_msg = buf + 6;
            fprintf(stderr, "Server Error: %s\n", error_msg);
            return EXIT_FAILURE;
        } else {
            fprintf(stderr, "Unexpected server response: %s\n", buf);
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Invalid arguments.\n\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    return 0;
}