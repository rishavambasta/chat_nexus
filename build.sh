#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

QT_PATH="/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64"

if [ -f "$QT_PATH/bin/qmake" ]; then
    QMAKE_BIN="$QT_PATH/bin/qmake"
elif which qmake >/dev/null 2>&1; then
    QMAKE_BIN="$(which qmake)"
else
    echo "Error: qmake not found."
    exit 1
fi

echo "==> Using qmake: $QMAKE_BIN"
echo "==> Configuring P2PChat..."
"$QMAKE_BIN" P2PChat.pro -spec linux-g++

echo "==> Compiling P2PChat..."
make -j$(nproc)

echo "==> Build successful! Executable is at $SCRIPT_DIR/P2PChat"
