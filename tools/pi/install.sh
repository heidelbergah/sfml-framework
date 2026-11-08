#!/usr/bin/env bash
#
# install.sh  -  Mud Cats on a Raspberry Pi, as a console
#
#   bash tools/pi/install.sh            start the game when the Pi starts
#   bash tools/pi/install.sh --remove   stop doing that
#
# Run it on the Pi, from the repository, once the game is built
# (build-cmake/mudcats/MudCats, from the CMake commands in the README).
#
# What it does: when the desktop starts after the Pi boots (Raspberry Pi
# OS logs you in by itself), the game starts too, fullscreen. If the game
# ever crashes it starts again a moment later. Quitting from the title
# screen's QUIT leaves you at the desktop, so you can still get at the Pi.
#
# On a Pi the game also wants a controller: until one is plugged in or
# switched on, it asks for one. Both of those are settings in
# ~/.config/mudcats/settings.txt (require_pad, fullscreen) if you ever
# want them off.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME="$ROOT/build-cmake/mudcats/MudCats"
LAUNCHER="$HOME/.local/bin/mudcats-console"
AUTOSTART="$HOME/.config/autostart/mudcats.desktop"

if [ "${1:-}" = "--remove" ]; then
    rm -f "$LAUNCHER" "$AUTOSTART"
    echo "Mud Cats won't start by itself any more."
    exit 0
fi

if [ ! -x "$GAME" ]; then
    echo "The game isn't built yet: $GAME is missing."
    echo "From $ROOT run:"
    echo "  cmake -B build-cmake -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build build-cmake --config Release -j4"
    exit 1
fi

# The launcher: the game, again and again if it crashes, until it's quit properly.
mkdir -p "$(dirname "$LAUNCHER")"
cat > "$LAUNCHER" <<EOF
#!/usr/bin/env bash
# Mud Cats, as the Pi's console. Made by tools/pi/install.sh.
cd "$(dirname "$GAME")"
while true; do
    "$GAME" && break        # quit from the title: back to the desktop
    sleep 2                 # crashed: start again
done
EOF
chmod +x "$LAUNCHER"

# Started with the desktop.
mkdir -p "$(dirname "$AUTOSTART")"
cat > "$AUTOSTART" <<EOF
[Desktop Entry]
Type=Application
Name=Mud Cats
Comment=Start Mud Cats when the Pi starts
Exec=$LAUNCHER
Terminal=false
X-GNOME-Autostart-enabled=true
EOF

# Fullscreen from the start. The game's own settings file, made now if it
# doesn't exist yet; an existing one is left alone except for this.
SETTINGS="${XDG_CONFIG_HOME:-$HOME/.config}/mudcats/settings.txt"
mkdir -p "$(dirname "$SETTINGS")"
if [ -f "$SETTINGS" ] && grep -q '^fullscreen ' "$SETTINGS"; then
    sed -i 's/^fullscreen .*/fullscreen 1/' "$SETTINGS"
else
    echo "fullscreen 1" >> "$SETTINGS"
fi

echo "Done. Mud Cats will start with the Pi from now on."
echo "Restart to try it:  sudo reboot"
echo "To undo:            bash tools/pi/install.sh --remove"
