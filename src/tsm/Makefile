# SPDX-License-Identifier: MIT
# Makefile for libtsm

ROOT = ../..
include $(ROOT)/common.mk

# Library name
LIB = libtsm.a

# Source files
SRCS = tsm-render.c \
       tsm-screen.c \
       tsm-selection.c \
       tsm-unicode.c \
       tsm-vte-charsets.c \
       tsm-vte.c

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
tsm-render.o: tsm-render.c libtsm.h libtsm-int.h
tsm-screen.o: tsm-screen.c libtsm.h libtsm-int.h
tsm-selection.o: tsm-selection.c libtsm.h libtsm-int.h
tsm-unicode.o: tsm-unicode.c libtsm.h libtsm-int.h
tsm-vte-charsets.o: tsm-vte-charsets.c libtsm.h libtsm-int.h
tsm-vte.o: tsm-vte.c libtsm.h libtsm-int.h
