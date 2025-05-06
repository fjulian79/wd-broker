/*
 * common.c – shared code for wd-broker and wd-client
 *
 * Part of wd-broker, a minimalistic watchdog supervisor daemon that acts as the
 * single source of truth for hardware watchdog feeding on embedded Linux systems.
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define WD_READ_TIMEOUT_MS 500 // timeout used in read_with_timeout

void fatal_errno(const char *context) {
    fprintf(stderr, "Error: %s (%s)\n", context, strerror(errno));
    exit(EXIT_FAILURE);
}

void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

int read_with_timeout(int fd, char *buffer, size_t buffer_size, bool stop_at_nl) {
    fd_set         read_fds;
    struct timeval timeout;
    ssize_t        bytes_read = 0;
    size_t         total_read = 0;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    timeout.tv_sec = WD_READ_TIMEOUT_MS / 1000;
    timeout.tv_usec = (WD_READ_TIMEOUT_MS % 1000) * 1000;

    while (total_read < buffer_size - 1) {
        int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret < 0) {
            // Error in select
            return -1;
        } else if (ret == 0) {
            // Timeout
            return 0;
        }

        bytes_read = read(fd, buffer + total_read, 1);
        if (bytes_read < 0) {
            // Error in read
            return -1;
        } else if (bytes_read == 0) {
            // EOF
            break;
        }

        total_read += bytes_read;
        if (stop_at_nl && buffer[total_read - 1] == '\n') {
            break;
        }
    }

    buffer[total_read] = '\0';
    return (int)total_read;
}