AC_INIT([wd-broker], [0.27.0], [https://github.com/fjulian79/wd-broker/issues])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_ARG_WITH([socket-path],
    AS_HELP_STRING([--with-socket-path=PATH], [default Unix domain socket path]),
    [socket_path="$withval"],
    [socket_path="/run/wd-broker.sock"]
)
AC_DEFINE_UNQUOTED(SOCKET_PATH_DEFAULT, ["$socket_path"], [Default path to the Unix domain socket])

AC_PROG_CC
CFLAGS="$CFLAGS -Wall -Werror"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
