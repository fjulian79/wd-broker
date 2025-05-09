/*
 * wd-client – C library for interacting with the wd-broker watchdog daemon.
 *
 * Provides a simple and robust client interface for registration, heartbeat,
 * and unregistration via a structured UNIX domain socket protocol.
 *
 * Designed for integration into embedded Linux applications with strict
 * watchdog coordination requirements. Offers clear status reporting and
 * timeout-aware socket handling.
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
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common.h"
#include "wd-client.h"

#define WD_SET_STATUS(client, fmt, ...)                                                            \
    snprintf((client)->status, sizeof((client)->status), (fmt), ##__VA_ARGS__)

/**
 * Establishes a connection to the wd-broker socket.
 * Sets the socket_fd in the wd_client_t instance.
 * Updates the status field with the result of the operation.
 *
 * @param client Pointer to the wd_client_t instance.
 * @return WD_CLIENT_OK on success, WD_CLIENT_ERR on error, WD_CLIENT_EPARAM for invalid parameters.
 */
static int wd_client_connect(wd_client_t *client) {
    int socket_fd = -1;

    if (!client || !client->socket_path[0]) {
        WD_SET_STATUS(client, "Invalid parameters");
        return WD_CLIENT_EPARAM;
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        WD_SET_STATUS(client, "Failed to create socket: %s", strerror(errno));
        return WD_CLIENT_ERR;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", client->socket_path);
    if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(socket_fd);
        socket_fd = -1;
        WD_SET_STATUS(client, "Failed to connect to socket %s: %s", client->socket_path,
                      strerror(errno));
        return WD_CLIENT_ERR;
    }

    WD_SET_STATUS(client, "Successfully connected via %s", client->socket_path);
    return socket_fd;
}

int wd_client_check_prot_version(wd_client_t *client) {
    int     socket_fd = -1;
    char    buffer[128] = {0};
    ssize_t len = 0;

    socket_fd = wd_client_connect(client);
    if (socket_fd < 0) {
        return socket_fd;
    }

    if (write(socket_fd, "VERSION\n", 8) < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to send VERSION command: %s", strerror(errno));
        return WD_CLIENT_ERR;
    }

    len = read_with_timeout(socket_fd, buffer, sizeof(buffer) - 1, false);
    if (len <= 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to read VERSION response");
        return WD_CLIENT_ERR;
    }

    buffer[len] = '\0';
    const char *prefix = "protocol_version=";
    char       *ver = strstr(buffer, prefix);
    if (!ver) {
        close(socket_fd);
        WD_SET_STATUS(client, "VERSION response malformed: %s", buffer);
        return WD_CLIENT_ERR;
    }
    ver += strlen(prefix);
    if (strncmp(ver, SOCKET_PROT_VERSION, strlen(SOCKET_PROT_VERSION)) != 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Incompatible protocol version %s, expect %s", ver,
                      SOCKET_PROT_VERSION);
        return WD_CLIENT_EVERSION;
    }

    close(socket_fd);
    return WD_CLIENT_OK;
}

int wd_client_register(wd_client_t *client, unsigned int timeout_ms, bool ignore_pid) {
    int socket_fd = -1;
    int ret = WD_CLIENT_OK;

    if (!client || !client->name[0]) {
        WD_SET_STATUS(client, "Invalid parameters");
        return WD_CLIENT_EPARAM;
    }

    if (timeout_ms < WD_CLIENT_TIMEOUT_MIN_MS || timeout_ms > WD_CLIENT_TIMEOUT_MAX_MS) {
        WD_SET_STATUS(client, "Invalid timeout value");
        return WD_CLIENT_EPARAM;
    }

    ret = wd_client_check_prot_version(client);
    if (ret != WD_CLIENT_OK) {
        return ret;
    }

    socket_fd = wd_client_connect(client);
    if (socket_fd < 0) {
        return socket_fd;
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "REGISTER %s %u%s\n", client->name, timeout_ms,
             ignore_pid ? " ignorepid" : "");
    if (write(socket_fd, cmd, strlen(cmd)) < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to send REGISTER command: %s", strerror(errno));
        return WD_CLIENT_ERR;
    }

    char    response[128];
    ssize_t len = read_with_timeout(socket_fd, response, sizeof(response) - 1, true);
    if (len < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to read REGISTER response: %s", strerror(errno));
        return WD_CLIENT_ERR;
    } else if (len == 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Timeout while waiting for REGISTER response");
        return WD_CLIENT_ETIMEOUT;
    }

    response[len] = '\0';
    if (strncmp(response, "OK ", 3) == 0) {
        strncpy(client->clientID, response + 3, sizeof(client->clientID) - 1);
        client->clientID[sizeof(client->clientID) - 1] = '\0'; // Nullterminierung sicherstellen
        close(socket_fd);
        WD_SET_STATUS(client, "Client registered successfully with ID: %s", client->clientID);
        return WD_CLIENT_OK;
    }

    close(socket_fd);
    WD_SET_STATUS(client, "REGISTER failed at wd-broker: %s", response);
    return WD_CLIENT_ERR;
}

int wd_client_ping(wd_client_t *client) {
    int socket_fd = -1;

    if (!client || !client->clientID[0]) {
        WD_SET_STATUS(client, "Invalid parameters");
        return WD_CLIENT_EPARAM;
    }

    socket_fd = wd_client_connect(client);
    if (socket_fd < 0) {
        return socket_fd;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "PING %s\n", client->clientID);
    if (write(socket_fd, cmd, strlen(cmd)) < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to send PING command: %s", strerror(errno));
        return WD_CLIENT_ERR;
    }

    char    response[64];
    ssize_t len = read(socket_fd, response, sizeof(response) - 1);
    if (len < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to read PING response: %s", strerror(errno));
        return WD_CLIENT_ERR;
    } else if (len == 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Timeout while waiting for PING response");
        return WD_CLIENT_ETIMEOUT;
    }

    response[len] = '\0';
    if (strncmp(response, "OK", 2) == 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "PING successful for client %s (%s) ", client->name,
                      client->clientID);
        return WD_CLIENT_OK;
    }

    close(socket_fd);
    WD_SET_STATUS(client, "PING failed at wd-broker: %s", response);
    return WD_CLIENT_ERR;
}

int wd_client_unregister(wd_client_t *client) {
    int socket_fd = -1;

    if (!client || !client->clientID[0]) {
        WD_SET_STATUS(client, "Invalid parameters");
        return WD_CLIENT_EPARAM;
    }

    socket_fd = wd_client_connect(client);
    if (socket_fd < 0) {
        return socket_fd;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "UNREGISTER %s\n", client->clientID);
    if (write(socket_fd, cmd, strlen(cmd)) < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to send UNREGISTER command: %s", strerror(errno));
        return WD_CLIENT_ERR;
    }

    char    response[64];
    ssize_t len = read_with_timeout(socket_fd, response, sizeof(response) - 1, true);
    if (len < 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Failed to read UNREGISTER response: %s", strerror(errno));
        return WD_CLIENT_ERR;
    } else if (len == 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "Timeout while waiting for UNREGISTER response");
        return WD_CLIENT_ETIMEOUT;
    }

    response[len] = '\0';
    if (strncmp(response, "OK", 2) == 0) {
        close(socket_fd);
        WD_SET_STATUS(client, "UNREGISTER successful for client %s (%s) ", client->name,
                      client->clientID);
        return WD_CLIENT_OK;
    }

    close(socket_fd);
    WD_SET_STATUS(client, "UNREGISTER failed at wd-broker: %s", response);
    return WD_CLIENT_ERR;
}
