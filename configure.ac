AC_INIT([wd-broker], [0.29.0], [https://github.com/fjulian79/wd-broker/issues])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

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
AC_DEFINE_UNQUOTED(MAX_CLIENTS, [$max_clients], [Maximum number of simultaneous clients])

AC_PROG_CC
CFLAGS="$CFLAGS -Wall -Werror"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
