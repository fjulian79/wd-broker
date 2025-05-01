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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/run/wd-broker.sock"
#define BUF_SIZE    4096

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
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
    if (argc != 2 && !(argc == 3 && strcmp(argv[1], "unregister") == 0)) {
        fprintf(stderr, "Usage: %s status | unregister <clientID>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *cmd = NULL;
    char        linebuf[128];

    if (strcmp(argv[1], "status") == 0) {
        cmd = "STATUS\n";
    } else if (strcmp(argv[1], "unregister") == 0 && argv[2]) {
        snprintf(linebuf, sizeof(linebuf), "UNREGISTER %s\n", argv[2]);
        cmd = linebuf;
    } else {
        fprintf(stderr, "Invalid arguments.\n");
        return EXIT_FAILURE;
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        die("socket");

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        die("connect");
    }

    if (write(sock, cmd, strlen(cmd)) < 0) {
        close(sock);
        die("write");
    }

    char    buf[BUF_SIZE];
    ssize_t total_len = 0;
    ssize_t len;
    while ((len = read(sock, buf + total_len, sizeof(buf) - 1 - total_len)) > 0) {
        total_len += len;
    }
    buf[total_len] = '\0';
    close(sock);

    char *lines[128];
    int   line_count = 0;

    char *saveptr = NULL;
    char *line = strtok_r(buf, "\n", &saveptr);
    while (line && line_count < 128) {
        lines[line_count++] = line;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    int has_client_rows = 0;
    for (int i = 0; i < line_count; ++i) {
        char id_check[17] = {0};
        if (sscanf(lines[i], "%16[0-9a-f]", id_check) == 1 && strlen(id_check) == 16) {
            if (!has_client_rows) {
                print_table_header();
                has_client_rows = 1;
            }
            print_line_formatted(lines[i]);
        } else {
            printf("%s\n", lines[i]);
        }
    }

    return 0;
}
