# SPDX-License-Identifier: MIT
# Makefile for libtsm tests
#
# NOTE: These tests use a minimal Check framework stub (test/check.h)
# No external Check library is required for IRIX builds

ROOT = ..
include $(ROOT)/common.mk

# Additional includes for tests (check.h is in this directory)
TEST_CFLAGS = $(CFLAGS) -I.

# Libraries to link against
LIBS = $(ROOT)/src/tsm/libtsm.a \
       $(ROOT)/src/shared/libshl.a \
       $(ROOT)/external/wcwidth/libwcwidth.a

# Test executables
TESTS = test_htable \
        test_screen \
        test_selection \
        test_symbol \
        test_valgrind \
        test_vte \
        test_vte_mouse

# Build targets
all: $(TESTS)

# test_htable only needs shl, not full libtsm
test_htable: test_htable.o
	$(CC) $(CFLAGS) -o $@ test_htable.o \
		$(ROOT)/src/shared/libshl.a

# test_valgrind is simple (no libtsm needed)
test_valgrind: test_valgrind.o
	$(CC) $(CFLAGS) -o $@ test_valgrind.o

# All other tests need full libtsm + dependencies
test_screen: test_screen.o
	$(CC) $(CFLAGS) -o $@ test_screen.o \
		$(LIBS) -lm

test_selection: test_selection.o
	$(CC) $(CFLAGS) -o $@ test_selection.o \
		$(LIBS) -lm

test_symbol: test_symbol.o
	$(CC) $(CFLAGS) -o $@ test_symbol.o \
		$(LIBS) -lm

test_vte: test_vte.o
	$(CC) $(CFLAGS) -o $@ test_vte.o \
		$(LIBS) -lm

test_vte_mouse: test_vte_mouse.o
	$(CC) $(CFLAGS) -o $@ test_vte_mouse.o \
		$(LIBS) -lm

# Compilation rules
.c.o:
	$(CC) $(TEST_CFLAGS) -c $< -o $@

clean:
	$(RM) $(TESTS) *.o

# Dependencies
test_htable.o: test_htable.c test_common.h
test_screen.o: test_screen.c test_common.h
test_selection.o: test_selection.c test_common.h
test_symbol.o: test_symbol.c test_common.h
test_valgrind.o: test_valgrind.c test_common.h
test_vte.o: test_vte.c test_common.h
test_vte_mouse.o: test_vte_mouse.c test_common.h
