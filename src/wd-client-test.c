/*
 * wd-client-test.c – Unit test for the wd-client library.
 *
 * Verifies correct behavior of client registration, heartbeat handling,
 * and client unregistration via the wd-broker interface.
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

#include "wd-client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WD_HEARTBEAT_TIMEOUT_MS 5000 // Timeout for heartbeat in milliseconds

#define WD_TEST_CLIENT_INIT                                                                        \
    {                                                                                              \
        .socket_path = "/tmp/wd-broker-test.sock", .name = "test-client", .clientID = {0},         \
        .status = {                                                                                \
            0                                                                                      \
        }                                                                                          \
    }

#define ASSERT_WITH_STATUS(expr, client)                                                           \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "Assertion failed: %s\nwd-client lib Status: %s\n", #expr,             \
                    (client).status);                                                              \
            assert(expr);                                                                          \
        }                                                                                          \
    } while (0)

void test_wd_client_register() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    result = wd_client_register(&client, WD_HEARTBEAT_TIMEOUT_MS, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_OK, client);
    ASSERT_WITH_STATUS(strlen(client.clientID) > 0, client);

    result = wd_client_unregister(&client);
    ASSERT_WITH_STATUS(result == WD_CLIENT_OK, client);

    printf("test_wd_client_register passed\n");
}

void test_wd_client_ping() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    result = wd_client_register(&client, WD_HEARTBEAT_TIMEOUT_MS, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_OK, client);
    ASSERT_WITH_STATUS(strlen(client.clientID) > 0, client);

    result = wd_client_ping(&client);
    ASSERT_WITH_STATUS(result == WD_CLIENT_OK, client);

    result = wd_client_unregister(&client);
    ASSERT_WITH_STATUS(result == WD_CLIENT_OK, client);

    printf("test_wd_client_ping passed\n");
}

void test_register_with_invalid_name() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    strncpy(client.name, "", sizeof(client.name));
    result = wd_client_register(&client, WD_HEARTBEAT_TIMEOUT_MS, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_EPARAM, client);

    printf("test_register_with_invalid_name passed\n");
}

void test_register_with_invalid_timeout() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    strncpy(client.name, "test-client", sizeof(client.name));
    result = wd_client_register(&client, WD_CLIENT_TIMEOUT_MIN_MS - 1, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_EPARAM, client);

    result = wd_client_register(&client, WD_CLIENT_TIMEOUT_MAX_MS + 1, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_EPARAM, client);

    printf("test_register_with_invalid_timeout passed\n");
}

void test_unregister_without_registering() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    result = wd_client_unregister(&client);
    ASSERT_WITH_STATUS(result == WD_CLIENT_EPARAM, client);

    printf("test_unregister_without_registering passed\n");
}

void test_ping_without_registering() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    result = wd_client_ping(&client);
    ASSERT_WITH_STATUS(result == WD_CLIENT_EPARAM, client);

    printf("test_ping_without_registering passed\n");
}

void test_timeout_on_register() {
    wd_client_t client = WD_TEST_CLIENT_INIT;
    int         result = WD_CLIENT_ERR;

    strncpy(client.socket_path, "/tmp/nonexistent.sock", sizeof(client.socket_path) - 1);
    result = wd_client_register(&client, WD_HEARTBEAT_TIMEOUT_MS, false);
    ASSERT_WITH_STATUS(result == WD_CLIENT_ERR, client);

    printf("test_timeout_on_register passed\n");
}

int main() {
    printf("Running wd-client tests...\n");

    test_wd_client_register();
    test_wd_client_ping();
    test_register_with_invalid_name();
    test_register_with_invalid_timeout();
    test_unregister_without_registering();
    test_ping_without_registering();
    test_timeout_on_register();

    printf("All tests passed!\n");
    return 0;
}