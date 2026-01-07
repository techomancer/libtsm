#!/bin/sh

if [ -d "/usr/share/terminfo" ]; then
    TARGET_BASE="/usr/share/terminfo"
else
    TARGET_BASE="/usr/share/lib/terminfo"
fi

if [ ! -d "${TARGET_BASE}/d" ]; then
    echo "Creating ${TARGET_BASE}/d"
    mkdir -p "${TARGET_BASE}/d"
fi

echo "Compiling dash.ti..."
TERMINFO="${TARGET_BASE}" tic dash.ti
