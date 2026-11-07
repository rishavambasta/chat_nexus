#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PATH="/run/media/rishav/DS1/qt/qt5.15/5.14.2/gcc_64"

export LD_LIBRARY_PATH="$QT_PATH/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$QT_PATH/plugins:$QT_PLUGIN_PATH"

if [ ! -f "$SCRIPT_DIR/P2PChat" ]; then
    echo "Executable not found. Building first..."
    "$SCRIPT_DIR/build.sh"
fi

exec "$SCRIPT_DIR/P2PChat" "$@"
