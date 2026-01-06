# SPDX-License-Identifier: MIT
# Makefile for shared helper library (shl)

ROOT = ../..
include $(ROOT)/common.mk

# Library name
LIB = libshl.a

# Source files (including pty for dash terminal)
SRCS = shl-htable.c shl-pty.c shl-ring.c

# Object files
OBJS = $(SRCS:.c=.o)

# Build targets
all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(AR_FLAGS) $@ $(OBJS)
	$(RANLIB) $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(LIB)

# Dependencies
shl-htable.o: shl-htable.c shl-htable.h shl-macro.h
shl-pty.o: shl-pty.c shl-pty.h shl-ring.h shl-macro.h
shl-ring.o: shl-ring.c shl-ring.h shl-macro.h
