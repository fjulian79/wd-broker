/*
 * common.h – shared declarations for wd-broker and wd-client
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

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#define SOCKET_PROT_VERSION "0.3"
#define CMD_REGISTER        "REGISTER "
#define CMD_PING            "PING "
#define CMD_UNREGISTER      "UNREGISTER "
#define CMD_STATUS          "STATUS"
#define CMD_VERSION         "VERSION"

/* Print the error context along with errno information and exit */
void fatal_errno(const char *context);

/* Print the error message and exit */
void fatal_error(const char *fmt, ...);

/**
 * Reads data from a file descriptor with a timeout.
 *
 * @param fd File descriptor to read from.
 * @param buffer Buffer to store the read data.
 * @param buffer_size Size of the buffer.
 * @param stop_at_nl If true, stop reading at newline character.
 * @return Number of bytes read on success, 0 on timeout, -1 on error.
 */
int read_with_timeout(int fd, char *buffer, size_t buffer_size, bool stop_at_nl);

#endif /* COMMON_H */
