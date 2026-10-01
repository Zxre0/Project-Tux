#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Edit the local Lawin account/locker/shop while no game session is running.
# Usage: scripts/lawin-admin.sh info | set-name NAME | grant ID... | ...
# (see integrations/lawin/admin.cjs). Prints one JSON object.
set -euo pipefail
REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CONFIG=${FOL_LAWIN_CONFIG:-${XDG_CONFIG_HOME:-$HOME/.config}/fortnite-on-linux/backend/lawin.json}
NODE=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["node"])' "$CONFIG")
exec unshare --user --map-root-user --net -- /bin/sh -c \
    'ip link set lo up && exec "$0" "$@"' "$NODE" "$REPO/integrations/lawin/admin.cjs" "$CONFIG" "$@"
