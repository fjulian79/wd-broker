/*
 * wd-client – C library interface for communicating with the wd-broker daemon.
 *
 * Provides a simple and robust API for client registration, heartbeat, and
 * unregistration over a UNIX domain socket – optimized for embedded Linux systems.
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

#ifndef WD_CLIENT_H
#define WD_CLIENT_H

#include <stdbool.h>
#include <sys/un.h>

#include "config.h"

/* Max length of client name.
 *
 * ATTENTION: Please not that CLIENT_NAME_FMT_LEN_STR in wd-broker.c must be
 *            in sync with this defintion.
 */
#define WD_CLIENT_NAME_LEN   64
#define WD_CLIENTID_LEN      17  // 16 hex digits + null terminator
#define WD_CLIENT_TIMEOUT_MIN_MS   (5 * 1000)
#define WD_CLIENT_TIMEOUT_MAX_MS   (300 * 1000)
#define WD_CLIENT_STATUS_LEN 256 // Length of the status string for error messages
#define WD_CLIENT_SOCKPATH_LEN                                                                     \
    sizeof(((struct sockaddr_un *)0)->sun_path) // Length of the socket path as given by Linux

#define WD_CLIENT_OK       0  // Operation successful
#define WD_CLIENT_ERR      -1 // General error
#define WD_CLIENT_EPARAM   -2 // Invalid parameters
#define WD_CLIENT_ETIMEOUT -3 // Timeout occurred
#define WD_CLIENT_EVERSION -4 // Incompatible protocol version

/**
 * Structure representing a wd-client instance.
 * Contains all necessary information for interacting with the wd-broker.
 * The included status string is updated with the result of each operation in order to provide
 * detailed error messages for logging in the application context.
 */
typedef struct {
    char socket_path[WD_CLIENT_SOCKPATH_LEN]; // Path to the Unix domain socket
    char name[WD_CLIENT_NAME_LEN];            // Client name
    char clientID[WD_CLIENTID_LEN];           // 16 hex chars + null terminator
    char status[WD_CLIENT_STATUS_LEN];        // Status string for detailed error messages
} wd_client_t;

/**
 * Default initializer for the wd_client_t structure.
 *
 * ATTENTION: You may need to update the socket_path to match your system's configuration.
 */
#define WD_CLIENT_INIT                                                                             \
    {                                                                                              \
        .socket_path = SOCKET_PATH_DEFAULT, .name = {0}, .clientID = {0}, .status = {0}                                                                                          \
    }

/**
 * Registers the client with the wd-broker once the protocol version is checked.
 * If successful, sets the clientID in the wd_client_t instance.
 * Updates the status field with the result of the operation.
 *
 * ATTENTION: The broker will use the given timeout value as it is. It's recomended to use a
 * shorter PING intervall in your application to mitigate scheduling jitter or high system laod. 
 * One or two seconds less than the timeout value is a good choice, or even 50% of the timeout value.
 * It depends on your application needs and system design.
 *
 * @param client Pointer to the wd_client_t instance.
 * @param timeout_ms Timeout in milliseconds for the client.
 * @param ignore_pid If true, the broker will ignore PID checks.
 * @return WD_CLIENT_OK on success, or other error codes on failure.
 */
int wd_client_register(wd_client_t *client, unsigned int timeout_ms, bool ignore_pid);

/**
 * Sends a PING to the wd-broker to renew the client's heartbeat.
 * Updates the status field with the result of the operation.
 *
 * @param client Pointer to the wd_client_t instance.
 * @return WD_CLIENT_OK on success, WD_CLIENT_ERR on error, WD_CLIENT_FATAL if the PING fails
 * critically.
 */
int wd_client_ping(wd_client_t *client);

/**
 * Unregisters the client from the wd-broker.
 * Updates the status field with the result of the operation.
 *
 * @param client Pointer to the wd_client_t instance.
 * @return WD_CLIENT_OK on success, WD_CLIENT_ERR on error, WD_CLIENT_EPARAM for invalid parameters.
 */
int wd_client_unregister(wd_client_t *client);

#endif // WD_CLIENT_H