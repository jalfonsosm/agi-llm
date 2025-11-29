#!/bin/bash
# Run script for NAGI

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NAGI_EXE="$SCRIPT_DIR/build/nagi"

# Check if executable exists
if [ ! -f "$NAGI_EXE" ]; then
    echo "Error: nagi executable not found at $NAGI_EXE"
    echo "Run ./build.sh first to compile."
    exit 1
fi

# Run with game directory as argument, or current directory
GAME_DIR="${1:-.}"

echo "Starting NAGI..."
echo "Game directory: $GAME_DIR"
echo ""

cd "$GAME_DIR"
exec "$NAGI_EXE"
