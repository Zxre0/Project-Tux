#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Add Project Tux to the application menu (user-local, no root needed).
set -euo pipefail
REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$REPO/build/project-tux"
[[ -x $BIN ]] || { echo "Build first: ./build.sh" >&2; exit 1; }
APPS=${XDG_DATA_HOME:-$HOME/.local/share}/applications
ICONS=${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps
mkdir -p "$APPS" "$ICONS"
cp "$REPO/gui/project-tux.svg" "$ICONS/project-tux.svg"
cat >"$APPS/project-tux.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Project Tux
GenericName=Fortnite Launcher
Comment=Join a Project Tux multiplayer server
Exec="$BIN"
Icon=project-tux
Terminal=false
Categories=Game;
Actions=play;

[Desktop Action play]
Name=Play now
Exec="$BIN" --launch
EOF
update-desktop-database "$APPS" 2>/dev/null || true
echo "Installed $APPS/project-tux.desktop"
