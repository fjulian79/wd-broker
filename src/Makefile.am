bin_PROGRAMS = wd-broker wd-ctrl
wd_broker_SOURCES = wd-broker.c
wd_broker_LDADD = libwd-client.la
wd_ctrl_SOURCES = wd-ctrl.c
wd_ctrl_LDADD = libwd-client.la

lib_LTLIBRARIES = libwd-client.la
libwd_client_la_SOURCES = wd-client.c wd-client.h common.c
libwd_client_la_LDFLAGS = -version-info 1:0:0

check_PROGRAMS = wd-client-test
wd_client_test_SOURCES = wd-client-test.c
wd_client_test_LDADD = ../src/libwd-client.la

AM_CFLAGS = -I$(top_srcdir)/include