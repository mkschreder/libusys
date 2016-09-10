@CODE_COVERAGE_RULES@
includedir=$(prefix)/include/libusys/
lib_LTLIBRARIES=libusys.la
include_HEADERS=runqueue.h ulog.h uloop_process.h uloop_timeout.h usock.h ustream.h
libusys_la_SOURCES=runqueue.c ulog.c uloop.c uloop_process.c uloop_timeout.c usock.c ustream-fd.c ustream.c
libusys_la_CFLAGS=$(CODE_COVERAGE_CFLAGS) -D_GNU_SOURCE -std=gnu99 -Wall -Werror
