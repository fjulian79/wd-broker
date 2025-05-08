# Makefile.am – Automake build rules for wd-broker
#
#     Copyright 2025 Julian Friedrich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is provided on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Source repository: https://github.com/fjulian79/wd-broker

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