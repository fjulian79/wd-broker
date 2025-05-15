/*
 * common – Shared utility code for wd-broker and wd-client.
 *
 * Provides common functions like error handling, logging, and input reading.
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

int read_with_timeout(int fd, char *buffer, size_t buffer_size) {
    fd_set         read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    timeout.tv_sec = WD_READ_TIMEOUT_MS / 1000;
    timeout.tv_usec = (WD_READ_TIMEOUT_MS % 1000) * 1000;

    int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0) {
        return -1; // select() error
    } else if (ret == 0) {
        return 0; // timeout
    }

    ssize_t len = read(fd, buffer, buffer_size - 1);
    if (len <= 0) {
        return (int)len; // error or EOF
    }

    buffer[len] = '\0';
    return (int)len;
}
