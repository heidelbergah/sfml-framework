#!/usr/bin/env bash
#
# appimage.sh  -  Mud Cats, as a Linux AppImage
#
#   tools/package/appimage.sh [build directory]
#
# Makes MudCats-<version>-Linux-x86_64.AppImage in the build directory
# (build-cmake if none is given), from a CMake Release build that's already
# been built. An AppImage is one file that runs on most Linux systems: make
# it executable and double-click it.
#
# The game inside is the same as the zip's: SFML and its sound and font
# libraries are linked in, so all it needs from the system is X11, udev and
# the C library, which every Linux desktop has. It's built on an older
# Ubuntu (see .github/workflows/build.yml) so it runs on newer ones too.
#
# appimagetool is downloaded the first time, unless APPIMAGETOOL points at
# one already.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(cd "${1:-$ROOT/build-cmake}" && pwd)"
VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD/CMakeCache.txt")"
ARCH="${ARCH:-$(uname -m)}"
OUT="$BUILD/MudCats-$VERSION-Linux-$ARCH.AppImage"
APPDIR="$BUILD/AppDir"

# The game and everything it reads, where the launcher expects them.
rm -rf "$APPDIR"
cmake --install "$BUILD" --config Release --component game --prefix "$APPDIR/usr/share/mudcats"

# What an AppImage needs at its top: a launcher, a desktop entry, an icon.
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/share/mudcats/MudCats" "$@"
EOF
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/mudcats.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Mud Cats
Comment=An infantry demo
Exec=MudCats
Icon=mudcats
Categories=Game;ActionGame;
Terminal=false
X-AppImage-Version=$VERSION
EOF
cp "$ROOT/tools/package/mudcats.png" "$APPDIR/mudcats.png"
ln -sf mudcats.png "$APPDIR/.DirIcon"

# The tool that packs it.
TOOL="${APPIMAGETOOL:-$BUILD/appimagetool-$ARCH.AppImage}"
if [ ! -x "$TOOL" ]; then
    curl -fsSL -o "$TOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"
    chmod +x "$TOOL"
fi

# Run unpacked: build machines often have no FUSE to mount it with.
rm -f "$OUT"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH="$ARCH" "$TOOL" --no-appstream "$APPDIR" "$OUT"
echo "$OUT"
