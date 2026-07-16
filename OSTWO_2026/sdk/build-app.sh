#!/bin/sh
# build-app.sh - compile an OS/Two application into an OS/2 LX executable.
#
# Usage:   ./build-app.sh examples/hello.c
# Output:  examples/hello.exe  (runs on OS/Two via: exec hello.exe)
#
# Apps are ordinary C programs: use standard C (printf, fopen, ...) and/or
# the OS/2 API from <os2.h>. They build against the Open Watcom OS/2 C
# runtime and link as OS/2 LX executables, which run on the 32-bit OS/Two
# kernel and, in compatibility mode, on the 64-bit kernel.
#
# Requires Open Watcom v2 (the descendant of the compiler used for real
# OS/2 development). Point WATCOM_BIN at its binl64 directory:
#   export WATCOM_BIN=/opt/watcom/binl64
# Get it from https://github.com/open-watcom/open-watcom-v2 (the Linux
# x64 release asset is a self-extracting zip: unzip it into a folder).
set -e

SDK_DIR=$(cd "$(dirname "$0")" && pwd)
: "${WATCOM_BIN:?Set WATCOM_BIN to your Open Watcom binl64 directory}"
WATCOM_ROOT=$(dirname "$WATCOM_BIN")

SRC="$1"
[ -n "$SRC" ] || { echo "usage: $0 <source.c>"; exit 1; }
DIR=$(dirname "$SRC")
BASE=$(basename "$SRC" .c)

echo "Building $DIR/$BASE.exe (OS/2 LX, full C runtime) ..."
WATCOM="$WATCOM_ROOT" \
INCLUDE="$SDK_DIR/include:$WATCOM_ROOT/h" \
PATH="$WATCOM_BIN:$PATH" \
  "$WATCOM_BIN/wcl386" -bt=os2 -l=os2v2 -s \
    -i="$SDK_DIR/include" \
    -fe="$DIR/$BASE.exe" "$SRC"

echo
echo "Done: $DIR/$BASE.exe"
echo "To run it on OS/Two, embed it into the kernel RamFS:"
echo "  python3 $SDK_DIR/embed-app.py $DIR/$BASE.exe > ${BASE}_bin.h"
echo "then #include it in kernel.c and ramfs_write(\"$BASE.exe\", ...)."
echo "See sdk/README.md for the full walkthrough."
