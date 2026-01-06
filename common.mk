# SPDX-License-Identifier: MIT
# Common makefile definitions for IRIX build with MIPSpro 7.4.4

# Compiler and tools
CC = cc
AR = ar
RANLIB = true

# Project root directory (ROOT) must be set by each Makefile BEFORE including this file
# Each Makefile should set ROOT relative to its location before the include directive

# Common flags for MIPSpro 7.4.4
# -n32: Use new 32-bit ABI
# -mips3: MIPS III instruction set
# -c99: Enable C99 mode
# -O2: Optimization level 2
# -woff 1174,1209,1506: Suppress common warnings
CFLAGS_COMMON = -n32 -mips3 -c99 -O2 -woff 1174,1209,1506,3970

# Project-specific defines
# Note: IRIX/MIPSpro doesn't need _POSIX_C_SOURCE or _GNU_SOURCE
# Define __attribute__ as empty since MIPSpro doesn't support GCC attributes
DEFINES = -D'__attribute__(x)='

# Include paths (relative to ROOT)
# external/ is included (not external/xkbcommon) for <xkbcommon/xkbcommon-keysyms.h>
INCLUDES = -I$(ROOT)/src/tsm \
           -I$(ROOT)/src/shared \
           -I$(ROOT)/external/wcwidth \
           -I$(ROOT)/external

# Force include config.h for all compilations
# This ensures inline and __attribute__ compatibility macros are always defined
CONFIG_INCLUDE = -include $(ROOT)/config.h

# Combined CFLAGS
CFLAGS = $(CFLAGS_COMMON) $(DEFINES) $(CONFIG_INCLUDE) $(INCLUDES)

# Archive command
AR_FLAGS = cru

# Standard targets
.PHONY: all clean

# Utilities
RM = rm -f
RMDIR = rm -rf
