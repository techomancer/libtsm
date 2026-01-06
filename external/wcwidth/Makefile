# SPDX-License-Identifier: MIT
# Makefile for wcwidth library

ROOT = ../..
include $(ROOT)/common.mk

# Library name
LIB = libwcwidth.a

# Source files
SRCS = wcwidth.c

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
wcwidth.o: wcwidth.c wcwidth.h
