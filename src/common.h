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

#define SOCKET_PROT_VERSION "0.2"
#define CMD_REGISTER        "REGISTER "
#define CMD_PING            "PING "
#define CMD_UNREGISTER      "UNREGISTER "
#define CMD_STATUS          "STATUS"
#define CMD_VERSION         "VERSION"

/* Print the error context along with errno information and exit */
void fatal_errno(const char *context);

/* Print the error message and exit */
void fatal_error(const char *fmt, ...);

#endif /* COMMON_H */
