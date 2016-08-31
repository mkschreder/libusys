@CODE_COVERAGE_RULES@
check_PROGRAMS=usock
usock_SOURCES=usock.c
usock_CFLAGS=$(CODE_COVERAGE_CFLAGS) -I../src/ -std=c99 
usock_LDFLAGS=$(CODE_COVERAGE_LDFLAGS) -L../src/.libs -lusys 
TESTS=$(check_PROGRAMS)
