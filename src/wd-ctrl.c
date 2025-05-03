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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "config.h"

#define BUF_SIZE            4096

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_help(const char *progname) {
    printf("Usage: %s [OPTIONS] status | unregister <clientID>\n", progname);
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

int main(int argc, char *argv[]) {
    const char          *socket_path = SOCKET_PATH_DEFAULT;
    const char          *cmd = NULL;
    char                 linebuf[128];
    int                  sock = 0;
    struct sockaddr_un   addr = {0};
    char                 buf[BUF_SIZE];
    ssize_t              total_len = 0;
    ssize_t              len;
    char                *lines[128];
    int                  line_count = 0;
    char                *saveptr = NULL;
    char                *line = NULL;
    int                  has_client_rows = 0;
    int                  opt;
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

    if (strcmp(argv[optind], "status") == 0) {
        cmd = "STATUS\n";
    } else if (strcmp(argv[optind], "unregister") == 0 && argv[optind + 1]) {
        snprintf(linebuf, sizeof(linebuf), "UNREGISTER %s\n", argv[optind + 1]);
        cmd = linebuf;
    } else {
        fprintf(stderr, "Invalid arguments.\n\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

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
        close(sock);
        die("write");
    }

    while ((len = read(sock, buf + total_len, sizeof(buf) - 1 - total_len)) > 0) {
        total_len += len;
    }
    buf[total_len] = '\0';
    close(sock);

    line = strtok_r(buf, "\n", &saveptr);
    while (line && line_count < 128) {
        lines[line_count++] = line;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    for (int i = 0; i < line_count; ++i) {
        char id_check[17] = {0};
        if (sscanf(lines[i], "%16[0-9a-f]", id_check) == 1 && strlen(id_check) == 16) {
            if (!has_client_rows) {
                print_table_header();
                has_client_rows = 1;
            }
            print_line_formatted(lines[i]);
        } else {
            fprintf(stderr, "Server error: %s\n", lines[i]);
        }
    }

    return 0;
}
