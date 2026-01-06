# SPDX-License-Identifier: MIT
# Top-level Makefile for libtsm
# For IRIX with MIPSpro 7.4.4 and classic make/smake

ROOT = .
include $(ROOT)/common.mk

# Subdirectories to build in order
SUBDIRS = external/wcwidth \
          src/shared \
          src/tsm

# Optional test directory
TEST_DIR = test

.PHONY: all clean tests install help

# Default target: build static libraries and tests
all:
	@echo "Building libtsm for IRIX..."
	@for dir in $(SUBDIRS); do \
		echo "Building in $$dir..."; \
		(cd $$dir && $(MAKE)) || exit 1; \
	done
	@echo "Build complete. Static libraries created:"
	@echo "  - external/wcwidth/libwcwidth.a"
	@echo "  - src/shared/libshl.a"
	@echo "  - src/tsm/libtsm.a"
	@echo "Building tests..."
	@(cd $(TEST_DIR) && $(MAKE)) || exit 1
	@echo "Tests built successfully in test/"

# Build libraries and tests (alias for all)
tests: all

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@for dir in $(SUBDIRS) $(TEST_DIR); do \
		echo "Cleaning $$dir..."; \
		(cd $$dir && $(MAKE) clean 2>/dev/null) || true; \
	done
	@echo "Clean complete."

# Install header and library (adjust DESTDIR as needed)
DESTDIR = /usr/local
INSTALL = install
INSTALL_DATA = $(INSTALL) -m 644

install: all
	@echo "Installing libtsm to $(DESTDIR)..."
	$(INSTALL) -d $(DESTDIR)/include
	$(INSTALL) -d $(DESTDIR)/lib
	$(INSTALL_DATA) src/tsm/libtsm.h $(DESTDIR)/include/
	$(INSTALL_DATA) src/tsm/libtsm.a $(DESTDIR)/lib/
	@echo "Installation complete."
	@echo "Header: $(DESTDIR)/include/libtsm.h"
	@echo "Library: $(DESTDIR)/lib/libtsm.a"

# Display help information
help:
	@echo "libtsm Makefile for IRIX"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build static libraries (default)"
	@echo "  tests    - Build static libraries and test executables"
	@echo "  clean    - Remove all build artifacts"
	@echo "  install  - Install library and header to $(DESTDIR)"
	@echo "  help     - Display this help message"
	@echo ""
	@echo "Build system requirements:"
	@echo "  - MIPSpro 7.4.4 compiler (cc)"
	@echo "  - IRIX make or smake"
	@echo ""
	@echo "Note: Tests use a minimal Check stub (no external dependencies)"
	@echo ""
	@echo "Output libraries:"
	@echo "  - src/tsm/libtsm.a          (main library)"
	@echo "  - src/shared/libshl.a       (helper library)"
	@echo "  - external/wcwidth/libwcwidth.a (wcwidth library)"
