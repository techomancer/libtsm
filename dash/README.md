# Dash - Modern IRIX Terminal Emulator

Dash is a lightweight, modern terminal emulator designed specifically for SGI IRIX workstations. It combines the classic Motif look and feel with OpenGL acceleration to provide a smooth, responsive experience with unique visual flair.

## License
Dash is released under the 3-Clause BSD License. See [LICENSE](LICENSE) for details.

## Project Structure

*   **`dash.c`**: The core application logic. Handles Motif widget creation, the main event loop, OpenGL context management, and the primary rendering pipeline.
*   **`dash.h`**: Common data structures and function prototypes.
*   **`dash-*.c`**: Implementations for the various dynamic backgrounds (Aurora, Night, Sunset, etc.).
*   **`dash-themes.c`**: Color palette definitions (Solarized, VGA, etc.).
*   **`../src/tsm`**: The `libtsm` library, which handles the complex state machine of a VT100-VT520 compatible terminal.
*   **`../src/shared`**: Shared helpers, specifically `shl-pty.c` which contains the IRIX-specific PTY and STREAMS handling code.

## Design Choices

### Native IRIX Integration
Dash is built using **Motif 1.2**, ensuring it looks and behaves like a native IRIX application. It integrates with the SGI Desktop via File Type Rules (`.ftr`) and Toolchest menus (`.chest`).

### OpenGL Acceleration
Unlike traditional X11 terminals that use server-side font rendering, Dash uses **OpenGL** for all drawing operations.
*   **Texture Atlas**: Font glyphs are rasterized once using X11 calls and cached into a texture atlas. This allows for extremely fast rendering of text using textured quads.
*   **Batch Rendering**: The entire terminal grid is rendered using vertex arrays and a single `glDrawArrays` call (per pass). This drastically reduces the overhead of OpenGL function calls compared to immediate mode rendering.
*   **Dynamic Backgrounds**: The GPU is utilized to render procedural, animated backgrounds (like the "Aurora" or "Retro Sunset" effects) without consuming significant CPU resources.

### State Separation
Dash uses **libtsm** to separate the terminal emulation logic from the display logic. `libtsm` handles escape sequences, scrollback buffers, and screen state, while Dash focuses purely on taking that state and rendering it to the screen.

### IRIX PTY Handling
IRIX uses a System V style PTY subsystem with STREAMS. Dash implements a robust PTY handler adapted for IRIX that correctly manages `_getpty` and handles signal propagation (`SIGWINCH`) to ensure shells and applications like `vi` or `mc` behave correctly.

## How It Works

1.  **Initialization**: Dash initializes the X11 display, creates the Motif widget hierarchy, and sets up an OpenGL context on a `GLwMDrawingArea` widget.
2.  **PTY Spawn**: A pseudo-terminal is allocated. The child process forks, sets up the slave PTY (pushing STREAMS modules), and executes the user's shell.
3.  **Input Loop**:
    *   **Keyboard/Mouse**: X11 input events are captured by Dash and fed into `libtsm`. `libtsm` converts these into the appropriate escape sequences, which Dash writes to the PTY master file descriptor.
    *   **PTY Output**: When the shell produces output, Dash reads from the PTY master and feeds the raw bytes into `libtsm`. `libtsm` updates its internal screen buffer.
4.  **Rendering**:
    *   A timer or event triggers a redraw.
    *   Dash clears the screen and draws the active dynamic background.
    *   It iterates over the `libtsm` screen grid.
    *   For each cell, it updates a client-side vertex array with the glyph's texture coordinates and colors.
    *   The text layer is rendered in a single batch using `glDrawArrays`.
    *   The frame is swapped to the display.