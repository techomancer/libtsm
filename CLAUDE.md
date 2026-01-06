# IRIX Build System for libtsm

## Overview

This project has been fully converted to build natively on IRIX with MIPSpro 7.4.4 and classic IRIX make/smake.
All Linux-specific code has been replaced with IRIX-compatible versions.

## Important: IRIX-Only Build

**This is an IRIX-native build.** It requires:
- IRIX operating system (tested on IRIX 6.5 IP30)
- MIPSpro 7.4.4 compiler
- IRIX make or smake

To build on the IRIX machine:
```
cd /files/project/libtsm
make
```

For tests:
```
make tests
```

Or use the helper scripts (from Linux via telnet):
```
bash build-all.sh     # Build all libraries + dash
bash build-dash.sh    # Build just dash
```

## Build System Structure

### Top-Level Files

- `config.h` - Configuration header with MIPSpro compatibility macros
- `common.mk` - Common makefile definitions (included by all Makefiles)
- `Makefile` - Top-level makefile
- `build-dash.sh` - Helper script to build dash via telnet
- `build-all.sh` - Helper script to build all libraries + dash via telnet

### Key Modifications for IRIX

**Replaced Files (Linux → IRIX):**
- `src/shared/shl-macro.h` - Removed GCC statement expressions, using regular functions
- `src/shared/shl-llog.h` - Removed `##__VA_ARGS__` (not supported by MIPSpro)

**Reference Files (irix/ directory):**
- `irix/check.h` - Minimal Check framework stub (no external dependency!)
- `irix/shl-htable.h` - Hash table definitions
- `irix/shl-llog.h` - Original logging header
- `irix/shl-macro.h` - Original macro header (kept as reference)
- `irix/README` - Build system documentation

### Output Libraries

- `external/wcwidth/libwcwidth.a` - Wide character width calculation
- `src/shared/libshl.a` - Shared helper utilities (htable, pty, ring)
- `src/tsm/libtsm.a` - Main terminal state machine library

### Applications

- `dash/dash` - Modern IRIX terminal emulator using libtsm, Motif 1.2, and OpenGL

### Test Executables

Built in `test/` directory:
- test_htable
- test_screen
- test_selection
- test_symbol
- test_valgrind
- test_vte
- test_vte_mouse

## Build Commands

```bash
# Build static libraries only
make

# Build libraries + tests
make tests

# Clean all build artifacts
make clean

# Install to /usr/local (or override with DESTDIR=...)
make install

# Show help
make help
```

## Check Framework Stub

The tests use a minimal Check unit testing framework stub (`irix/check.h`) that provides just enough functionality to compile and run libtsm's tests without requiring the full Check library.

Supported Check API:
- `START_TEST()` / `END_TEST`
- `ck_assert()`, `ck_assert_int_eq()`, `ck_assert_ptr_ne()`, etc.
- `tcase_create()`, `tcase_add_test()`, `suite_create()`, etc.
- `srunner_create()`, `srunner_run_all()`, `srunner_free()`

## Compiler Flags

Default flags from `common.mk`:
```
-n32                    # Use new 32-bit ABI
-mips3                  # MIPS III instruction set
-c99                    # C99 mode
-O2                     # Optimization level 2
-woff 1174,1209,1506    # Suppress common MIPSpro warnings
-D'__attribute__(x)='   # Disable GCC attributes
```

Include paths (in order):
1. `src/tsm/` - Main library headers
2. `src/shared/` - Shared helper headers
3. `external/wcwidth/` - wcwidth headers
4. `external/` - For `<xkbcommon/xkbcommon-keysyms.h>`

Force-included: `config.h` via `-include` flag

## MIPsPro C99 Compatibility

MIPSpro 7.4.4's C99 support is incomplete. The build system handles these incompatibilities:

### GCC Attributes
- `__attribute__(...)` is defined as empty via `-D'__attribute__(x)='`
- MIPSpro doesn't support format checking, visibility, etc.

### Statement Expressions
- GCC `({ ... })` syntax NOT supported by MIPsPro
- `shl-macro.h` uses regular inline functions instead of statement expression macros

### Variadic Macros
- `##__VA_ARGS__` (GCC extension) is NOT supported by MIPSpro
- `shl-llog.h` uses alternative approach - moves format into `__VA_ARGS__`

### Other Compatibility
- `inline` is mapped to `__inline` in config.h
- `__func__` is mapped to `__FUNCTION__` in config.h

## Dash Terminal Emulator

Dash is a modern terminal emulator for IRIX featuring:
- **Motif 1.2 GUI** with menu bar, tab buttons, and scrollbar
- **OpenGL rendering** using GLwMDrawingArea widget for hardware-accelerated text
- **libtsm backend** for VT100/xterm terminal emulation
- **Texture atlas** for efficient glyph rendering with anti-aliasing (4x, 3x, 2x, or 1x)
- **Vertex arrays** for batch rendering (single glDrawArrays call per frame)
- **Proper texel centering** for crisp glyph rendering

### Dash Build

```bash
cd dash
make
```

### Dash Status

**Working:**
- Motif GUI with tabs and scrollbar
- OpenGL rendering with texture atlas
- Font loading with flexible anti-aliasing
- Vertex array batch rendering
- TSM screen and VTE integration
- Draw callback fills vertex arrays with glyph data and colors
- Background color tracking

**TODO:**
- shl-pty needs porting to IRIX (currently uses Linux `pty.h` and `epoll`)
- PTY I/O integration
- Keyboard input handling
- Text selection
- Scrollbar integration

## Notes

1. The build produces STATIC libraries (.a) only, not shared libraries
2. gtktsm is NOT built (GTK not available on IRIX)
3. Tests use stub Check framework (no external dependency)
4. Compatible with both `make` and `smake`
5. All IRIX-specific modifications are now the main codebase
