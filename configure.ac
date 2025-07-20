dnl configure.ac – Autotools configure script for wd-broker.
dnl
dnl     Copyright 2025 Julian Friedrich
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at:
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is provided on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl
dnl Source repository: https://github.com/fjulian79/wd-broker

AC_INIT([wd-broker], [2.1.0], [https://github.com/fjulian79/wd-broker/issues])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

LT_INIT
AM_PROG_AR
AC_PROG_CC

AC_ARG_WITH([socket-path],
    AS_HELP_STRING([--with-socket-path=PATH], [default Unix domain socket path]),
    [socket_path="$withval"],
    [socket_path="/run/wd-broker.sock"]
)
AC_DEFINE_UNQUOTED(SOCKET_PATH_DEFAULT, ["$socket_path"], [Default path to the Unix domain socket])

AC_ARG_WITH([service-user],
    AS_HELP_STRING([--with-service-user=USER], [default user name for the service]),
    [service_user="$withval"],
    [service_user="wd-broker"]
)
AC_DEFINE_UNQUOTED(SERVICE_USER_DEFAULT, ["$service_user"], [Default user name for the service])

AC_ARG_WITH([max-clients],
    AS_HELP_STRING([--with-max-clients=NUM], [maximum number of simultaneous clients]),
    [max_clients="$withval"],
    [max_clients=64]
)
AC_DEFINE_UNQUOTED(WD_MAX_CLIENTS, [$max_clients], [Maximum number of simultaneous clients])

CFLAGS="$CFLAGS -Wall -Werror"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile include/Makefile])
AC_OUTPUT
