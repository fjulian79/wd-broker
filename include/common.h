/*
 * common – Shared utility code for wd-broker and wd-client.
 *
 * Declares common functions like error handling, logging, and input reading.
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

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#define SOCKET_PROT_VERSION "1.0"
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
