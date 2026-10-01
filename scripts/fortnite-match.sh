#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Local 12.50 BR Solo match: Lawin backend + headless Erbium game server +
# rendered client, all inside one loopback-only network namespace.
#
#   scripts/fortnite-match.sh            # server + client
#   scripts/fortnite-match.sh --server   # server only (join from another client)
#
# Override paths with FOL_GAME_DIR, FOL_PROTON, FOL_ERBIUM_DLL if needed.
set -euo pipefail

REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
DATA=${XDG_DATA_HOME:-$HOME/.local/share}/fortnite-on-linux
CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}/fortnite-on-linux
STATE=${XDG_STATE_HOME:-$HOME/.local/state}/fortnite-on-linux
GAME_DIR=${FOL_GAME_DIR:-$HOME/Documents/Fortnite/++Fortnite+Release-12}
PROTON=${FOL_PROTON:-$DATA/runtimes/experimental-20260924/proton}
COMPAT=$DATA/proton-prefixes/12.50
ERBIUM=${FOL_ERBIUM_DLL:-$(ls "$REPO"/build-erbium/*/erbium-solo.dll 2>/dev/null | head -1)}
WIN64="$GAME_DIR/FortniteGame/Binaries/Win64"
LOGS="$COMPAT/pfx/drive_c/users/steamuser/AppData/Local/FortniteGame/Saved/Logs"
winpath() { printf 'Z:%s' "${1//\//\\}"; }

if [[ ${1:-} != --inner ]]; then
    for f in "$PROTON" "$ERBIUM" "$REPO/build/windows/fol-inject.exe" \
             "$REPO/build/windows/fol-match-router.dll" "$REPO/build/fortnite-backend" \
             "$WIN64/FortniteClient-Win64-Shipping.exe"; do
        [[ -e $f ]] || { echo "Missing: $f (run ./build.sh, helpers/windows/build.sh, Erbium build)" >&2; exit 1; }
    done
    SESSION=$(date -u +%Y-%m-%d_%H-%M-%S)_match
    export FOL_SESSION_ID=$SESSION FOL_MATCH_DIR="$STATE/logs/sessions/$SESSION"
    export FOL_MATCH_MODE=${1:---both}
    mkdir -p "$FOL_MATCH_DIR"
    echo "Session logs: $FOL_MATCH_DIR"
    OUTER=$(stat -L -c '%d:%i' /proc/self/ns/net)
    exec env -u OPENSSL_ia32cap STEAM_COMPAT_CLIENT_INSTALL_PATH="$COMPAT/empty-steam-client" \
        STEAM_COMPAT_DATA_PATH="$COMPAT" WINEPREFIX="$COMPAT/pfx" SteamAppId=0 SteamGameId=0 \
        PROTON_LOG=0 WINEDLLOVERRIDES='winemenubuilder.exe=d' NO_PROXY=127.0.0.1,localhost \
        no_proxy=127.0.0.1,localhost DXVK_LOG_PATH="$FOL_MATCH_DIR" \
        unshare --user --map-root-user --net -- "$REPO/build/fortnite-backend" \
        --config "$CONFIG/backend/identity.json" --log "$FOL_MATCH_DIR/backend.log" \
        --isolated-session --outer-netns "$OUTER" --lawin-config "$CONFIG/backend/lawin.json" \
        run -- /bin/bash "$REPO/scripts/fortnite-match.sh" --inner
fi

# ---- inside the isolated namespace, after Lawin is ready ----
S=$FOL_MATCH_DIR
read -r USERNAME PASSWORD < <(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d["username"], d["password"])' "$CONFIG/backend/local-login.json")
GAME_ARGS=(-epicportal -epicapp=Fortnite -epicenv=Prod -epiclocale=en-us -skippatchcheck -nobe
           -fromfl=eac -fltoken=3db3ba5dcbd2e16703f3978d
           "-AUTH_LOGIN=$USERNAME" "-AUTH_PASSWORD=$PASSWORD" -AUTH_TYPE=epic)
EXE=$(winpath "$WIN64/FortniteClient-Win64-Shipping.exe")
export FOL_HTTP_OBSERVER_LAYOUT='59236016,107847520,104,17' FOL_HTTP_ROUTER_LAYOUT='59245424,10'
export FOL_INJECT_EARLY=$(winpath "$REPO/build/windows/fol-match-router.dll")
cd "$WIN64"
pids=()
cleanup() {
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
    WINEPREFIX="$COMPAT/pfx" "$(dirname "$PROTON")/files/bin/wineserver" -k 2>/dev/null || true
    if [[ $FOL_MATCH_MODE == --client ]]; then
        cp -f "$LOGS"/FortniteGame.log "$S/client-game.log" 2>/dev/null || true
    else
        cp -f "$LOGS"/FortniteGame.log "$S/server-game.log" 2>/dev/null || true
        cp -f "$LOGS"/FortniteGame_2.log "$S/client-game.log" 2>/dev/null || true
    fi
    cp -f "$WIN64/stdout.log" "$S/erbium-stdout.log" 2>/dev/null || true
}
trap cleanup EXIT
trap "exit 143" TERM INT

# Test automation: fullscreen the client, enter BR if needed and press Play.
ui() {
    local code=0
    FOL_GAME_WINDOWS_PID=$client_windows_pid FOL_UI_ACTION=$1 \
        "$(dirname "$PROTON")/files/bin/wine" "$REPO/build/windows/fol-ui-input-probe.exe" \
        >>"$S/ui-input.log" 2>&1 || code=$?
    ((code)) && echo "ui $1 failed: $code" >>"$S/ui-input.log"
    return $code
}
shot() { grim "$S/$1.png" 2>/dev/null || true; }
autoplay() {
    set +e
    local log="$LOGS/FortniteGame_2.log" i
    for ((i = 0; i < 120; i++)); do
        client_windows_pid=$(sed -n 's/.*started pid \([0-9]*\).*/\1/p' "$S/inject-client.txt" 2>/dev/null | head -1)
        [[ -n $client_windows_pid ]] && grep -qs 'UI.State.Startup.SubgameSelect\|UI.State.Athena.Frontend' "$log" && break
        sleep 1
    done
    local linux_pid
    linux_pid=$(pgrep -f 'Win64-Shipping.exe' | grep -vx "${server_pid:-0}" | head -1)
    hyprctl dispatch focuswindow "pid:$linux_pid" >/dev/null && hyprctl dispatch fullscreen 0 >/dev/null
    sleep 5
    if ! grep -qs 'UI.State.Athena.Frontend' "$log"; then
        shot autoplay-1-subgame; ui select-br; sleep 3; ui enter-br; sleep 15
    fi
    shot autoplay-2-lobby
    for i in 1 2 3; do ui match-play && grep -qs 'Browse: 127.0.0.1' "$log" && break; sleep 10; done
    sleep 20
    shot autoplay-3-after-play; sleep 60; shot autoplay-4-match; sleep 60; shot autoplay-5-match
}

udp_bound() { grep -q ':1E61 ' /proc/net/udp /proc/net/udp6 2>/dev/null; }

if [[ $FOL_MATCH_MODE != --client ]]; then
    mkdir -p "$LOGS"
    rm -f "$LOGS/FortniteGame.log" "$LOGS/FortniteGame_2.log"
    rm -f "$WIN64/stdout.log"
    echo "Starting headless game server..."
    FOL_HTTP_MODULE_REPORT="$(winpath "$S")\\router-server.json" \
    FOL_INJECT_STATUS="$(winpath "$S")\\inject-server.txt" FOL_INJECT_LATE=$(winpath "$ERBIUM") \
    FOL_INJECT_LOG='C:\users\steamuser\AppData\Local\FortniteGame\Saved\Logs\FortniteGame.log' \
    FOL_INJECT_MARKER='UI State changing from [UI.State.Startup.Login]' \
    WINEDEBUG='+timestamp,+pid,+seh,+loaddll' \
        "$PROTON" run "$REPO/build/windows/fol-inject.exe" "$EXE" -log -nullrhi -nosplash -nosound \
        "${GAME_ARGS[@]}" >"$S/server-wine.log" 2>&1 &
    pids+=($!)
    for ((i = 0; i < 600; i++)); do
        udp_bound && break
        kill -0 "${pids[0]}" 2>/dev/null || { echo "Game server exited early; see $S" >&2; exit 1; }
        sleep 1
    done
    udp_bound || { echo "Game server never listened on 7777; see $S" >&2; exit 1; }
    echo "Game server listening on 127.0.0.1:7777 after ${i}s"
fi

if [[ $FOL_MATCH_MODE == --server ]]; then
    wait "${pids[0]}"
    exit $?
fi
[[ $FOL_MATCH_MODE == --client ]] && rm -f "$LOGS/FortniteGame.log"
echo "Starting client..."
server_pid=$(pgrep -f 'Win64-Shipping.exe' | head -1 || true)
FOL_HTTP_MODULE_REPORT="$(winpath "$S")\\router-client.json" WINEDEBUG='-all' \
FOL_INJECT_STATUS="$(winpath "$S")\\inject-client.txt" \
    "$PROTON" run "$REPO/build/windows/fol-inject.exe" "$EXE" -log -d3d11 "${GAME_ARGS[@]}" \
    >"$S/client-wine.log" 2>&1 &
client=$!
pids+=($client)
echo "Client running. In the lobby select Solo and press Play."
[[ ${FOL_MATCH_AUTOPLAY:-0} == 1 ]] && autoplay
code=0
wait "$client" || code=$?
echo "Client exited ($code)"
exit $code
