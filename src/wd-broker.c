/*
 * wd-broker, a minimalistic watchdog supervisor daemon that acts as the single 
 * source of truth for hardware watchdog feeding on embedded Linux systems.
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

 #define _GNU_SOURCE
 #include <stdio.h>
 #include <stdlib.h>
 #include <stdint.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/socket.h>
 #include <sys/un.h>
 #include <fcntl.h>
 #include <errno.h>
 #include <time.h>
 #include <signal.h>
 #include <getopt.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/timerfd.h>
 
 #include "config.h"  // for PACKAGE_VERSION
 
 #define SOCKET_PATH_DEFAULT "/tmp/wd-broker.sock"
 #define SOCKET_PATH_TEST "/tmp/wd-broker-test.sock"
 #define MAX_CLIENTS 64
 #define CLIENTID_LEN 9
 #define BUF_SIZE 128
 #define DEFAULT_LOOP_INTERVAL_MS 1000  // 1 second granularity
 
 // Struct for registered clients
 typedef struct {
     char clientID[CLIENTID_LEN];
     char name[64];
     int timeout_ms;
     struct timespec last_ping;
     int active;
 } client_t;
 
 char *socket_path = SOCKET_PATH_DEFAULT;
 client_t clients[MAX_CLIENTS];
 int watchdog_fd = -1;
 int running = 1;
 int watchdog_enabled = 0;
 int test_mode = 0;
 int loop_interval_ms = DEFAULT_LOOP_INTERVAL_MS;
 
 void print_help(const char *progname) {
     printf("Usage: %s [--test] [--help] [--version] [--interval <ms>]\n", progname);
     printf("\nOptions:\n");
     printf("  --test             Run in test mode (no /dev/watchdog access)\n");
     printf("  --interval <ms>    Set loop interval in milliseconds (default: %d)\n", DEFAULT_LOOP_INTERVAL_MS);
     printf("  --help             Show this help message\n");
     printf("  --version          Show version information\n");
 }
 
 void make_clientID(char *clientID_out) {
     static const char hex[] = "0123456789abcdef";
     for (int i = 0; i < CLIENTID_LEN - 1; ++i) {
         clientID_out[i] = hex[rand() % 16];
     }
     clientID_out[CLIENTID_LEN - 1] = '\0';
 }
 
 void get_now(struct timespec *ts) {
     clock_gettime(CLOCK_MONOTONIC, ts);
 }
 
 long ms_since(struct timespec *then) {
     struct timespec now;
     get_now(&now);
     return (now.tv_sec - then->tv_sec) * 1000 + (now.tv_nsec - then->tv_nsec) / 1000000;
 }
 
 void signal_handler(int sig) {
     running = 0;
 }
 
 void write_or_log(int fd, const char *msg, size_t len) {
     ssize_t written = write(fd, msg, len);
     if (written < 0) {
         perror("write");
     }
 }
 
 void write_str(int fd, const char *msg) {
     write_or_log(fd, msg, strlen(msg));
 }
 
 void handle_command(int client_sock) {
     char buf[BUF_SIZE];
     int len = read(client_sock, buf, sizeof(buf)-1);
     if (len <= 0) return;
     buf[len] = '\0';
 
     if (strncmp(buf, "REGISTER ", 9) == 0) {
         char name[64];
         int timeout;
         if (sscanf(buf + 9, "%63s %d", name, &timeout) != 2) {
             write_str(client_sock, "ERROR invalid REGISTER\n");
             return;
         }
         for (int i = 0; i < MAX_CLIENTS; ++i) {
             if (!clients[i].active) {
                 clients[i].active = 1;
                 strncpy(clients[i].name, name, sizeof(clients[i].name)-1);
                 clients[i].timeout_ms = timeout;
                 get_now(&clients[i].last_ping);
                 make_clientID(clients[i].clientID);
                 printf("INFO: client '%s' registered with timeout %d ms (clientID=%s)\n",
                        clients[i].name, clients[i].timeout_ms, clients[i].clientID);
                 dprintf(client_sock, "OK %s\n", clients[i].clientID);
                 return;
             }
         }
         write_str(client_sock, "ERROR too many clients\n");
     }
     else if (strncmp(buf, "PING ", 5) == 0) {
         char clientID[CLIENTID_LEN];
         sscanf(buf + 5, "%8s", clientID);
         for (int i = 0; i < MAX_CLIENTS; ++i) {
             if (clients[i].active && strncmp(clients[i].clientID, clientID, CLIENTID_LEN) == 0) {
                 get_now(&clients[i].last_ping);
                 write_str(client_sock, "OK\n");
                 return;
             }
         }
         write_str(client_sock, "ERROR unknown clientID\n");
     }
     else if (strncmp(buf, "UNREGISTER ", 11) == 0) {
         char clientID[CLIENTID_LEN];
         sscanf(buf + 11, "%8s", clientID);
         for (int i = 0; i < MAX_CLIENTS; ++i) {
             if (clients[i].active && strncmp(clients[i].clientID, clientID, CLIENTID_LEN) == 0) {
                 printf("INFO: client '%s' unregistered (clientID=%s)\n",
                        clients[i].name, clients[i].clientID);
                 clients[i].active = 0;
                 write_str(client_sock, "OK\n");
                 return;
             }
         }
         write_str(client_sock, "ERROR unknown clientID\n");
     } else {
         write_str(client_sock, "ERROR unknown command\n");
     }
 }
 
 int main(int argc, char *argv[]) {
     static struct option long_options[] = {
         {"test", no_argument, 0, 't'},
         {"help", no_argument, 0, 'h'},
         {"version", no_argument, 0, 'v'},
         {"interval", required_argument, 0, 'i'},
         {0, 0, 0, 0}
     };
 
     int opt;
     while ((opt = getopt_long(argc, argv, "thvi:", long_options, NULL)) != -1) {
         switch (opt) {
             case 't': test_mode = 1; break;
             case 'h': print_help(argv[0]); return 0;
             case 'v': printf("wd-broker v%s\n", PACKAGE_VERSION); return 0;
             case 'i': loop_interval_ms = atoi(optarg); break;
             default: print_help(argv[0]); return 1;
         }
     }
 
     if (!test_mode) {
         pid_t pid = fork();
         if (pid < 0) {
             perror("fork");
             return 1;
         }
         if (pid > 0) {
             return 0; // parent exits
         }
         setsid();
         chdir("/");
         fclose(stdin);
         fclose(stdout);
         fclose(stderr);
     } else {
         socket_path = SOCKET_PATH_TEST;
         printf("Running in test mode. /dev/watchdog will not be used.\n");
         printf("Using socket path: %s\n", socket_path);
     }
 
     srand(time(NULL));
     signal(SIGINT, signal_handler);
     signal(SIGTERM, signal_handler);
 
     unlink(socket_path);
     int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
     struct sockaddr_un addr = {.sun_family = AF_UNIX};
     strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
     bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
     listen(server_sock, 5);
 
     chmod(socket_path, 0666);
 
     if (!test_mode) {
         watchdog_fd = open("/dev/watchdog", O_WRONLY);
         if (watchdog_fd >= 0) {
             watchdog_enabled = 1;
         } else {
             perror("Failed to open /dev/watchdog");
             exit(EXIT_FAILURE);
         }
     }
 
     int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
     struct itimerspec its = {
         .it_interval = { .tv_sec = loop_interval_ms / 1000, .tv_nsec = (loop_interval_ms % 1000) * 1000000 },
         .it_value    = { .tv_sec = loop_interval_ms / 1000, .tv_nsec = (loop_interval_ms % 1000) * 1000000 }
     };
     timerfd_settime(timer_fd, 0, &its, NULL);
 
     while (running) {
         fd_set fds;
         FD_ZERO(&fds);
         FD_SET(server_sock, &fds);
         FD_SET(timer_fd, &fds);
         int maxfd = (server_sock > timer_fd ? server_sock : timer_fd) + 1;
 
         struct timeval timeout = { .tv_sec = 0, .tv_usec = 500000 }; // 500ms
         int ret = select(maxfd, &fds, NULL, NULL, &timeout);
 
         if (ret > 0) {
             if (FD_ISSET(server_sock, &fds)) {
                 int client_sock = accept(server_sock, NULL, NULL);
                 if (client_sock >= 0) {
                     handle_command(client_sock);
                     close(client_sock);
                 }
             }
             if (FD_ISSET(timer_fd, &fds)) { 
                uint64_t expirations;
                 read(timer_fd, &expirations, sizeof(expirations));
 
                 int all_ok = 1;
                 for (int i = 0; i < MAX_CLIENTS; ++i) {
                     if (clients[i].active) {
                         long age = ms_since(&clients[i].last_ping);
                         if (age > clients[i].timeout_ms) {
                             printf("WARNING: client '%s' (clientID=%s) missed heartbeat (%ld ms > %d ms)\n",
                                    clients[i].name, clients[i].clientID, age, clients[i].timeout_ms);
                             all_ok = 0;
                             break;
                         }
                     }
                 }
                 if (watchdog_enabled && all_ok) {
                     write(watchdog_fd, "V", 1);
                 }
             }
         }
     }
 
     if (watchdog_enabled) {
         write(watchdog_fd, "V", 1);
         close(watchdog_fd);
     }
     close(timer_fd);
     unlink(socket_path);
     return 0;
 }
 