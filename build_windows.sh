#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

QT_WIN_DIR="/run/media/rishav/DS1/prog/chat/qt_win/6.5.3/mingw_64"
TOOLCHAIN_PREFIX="/run/media/rishav/DS1/prog/chat/tools/gcc-mingw"

echo "============================================="
echo " Building P2PChat for Windows (Qt 6.5.3 MinGW)"
echo "============================================="

# Ensure wrappers are executable
chmod +x "$SCRIPT_DIR/tools/wrappers/moc" "$SCRIPT_DIR/tools/wrappers/rcc" "$SCRIPT_DIR/tools/wrappers/uic"

NINJA_BIN="$(which ninja 2>/dev/null || echo "$HOME/.local/bin/ninja")"

echo "==> Configuring with CMake and Ninja..."
cmake -G Ninja -B build_win -S . \
    -DCMAKE_TOOLCHAIN_FILE="$SCRIPT_DIR/cmake/mingw_windows_toolchain.cmake" \
    -DCMAKE_MAKE_PROGRAM="$NINJA_BIN"

echo "==> Compiling Windows binaries..."
cmake --build build_win -j$(nproc)

echo "==> Deploying standalone Windows distribution in dist_windows/..."
mkdir -p "$SCRIPT_DIR/dist_windows"
cp "$SCRIPT_DIR/build_win/P2PChat.exe" "$SCRIPT_DIR/dist_windows/"

# Deploy Qt DLLs and plugins using windeployqt
WINEPATH="$QT_WIN_DIR/bin:$TOOLCHAIN_PREFIX/x86_64-w64-mingw32/bin" \
wine "$QT_WIN_DIR/bin/windeployqt.exe" --no-compiler-runtime "$SCRIPT_DIR/dist_windows/P2PChat.exe"

# Copy MinGW runtime DLLs
cp "$TOOLCHAIN_PREFIX/x86_64-w64-mingw32/bin/libwinpthread-1.dll" "$SCRIPT_DIR/dist_windows/"
cp "$TOOLCHAIN_PREFIX/x86_64-w64-mingw32/lib/libstdc++-6.dll" "$SCRIPT_DIR/dist_windows/"
cp "$TOOLCHAIN_PREFIX/x86_64-w64-mingw32/lib/libgcc_s_seh-1.dll" "$SCRIPT_DIR/dist_windows/"

# Create launch helper batch script
cat << 'EOF' > "$SCRIPT_DIR/dist_windows/run_p2pchat.bat"
@echo off
start "" "%~dp0P2PChat.exe" %*
EOF

echo "============================================="
echo " Windows build and deployment complete!"
echo " Executable: dist_windows/P2PChat.exe"
echo " Standalone bundle ready in: $SCRIPT_DIR/dist_windows"
echo "============================================="
