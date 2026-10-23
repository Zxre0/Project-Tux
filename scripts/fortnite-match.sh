#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Local BR Solo match: Lawin backend + headless Erbium game server +
# rendered client, all inside one loopback-only network namespace.
#
#   scripts/fortnite-match.sh            # server + client
#   scripts/fortnite-match.sh --server   # server only (join from another client)
#
# The build's release string selects profiles/fortnite/<version>.toml.
# Override with FOL_GAME_DIR, FOL_PROTON, FOL_ERBIUM_DLL, FOL_VERSION if needed.
set -euo pipefail

REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
DATA=${XDG_DATA_HOME:-$HOME/.local/share}/fortnite-on-linux
CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}/fortnite-on-linux
STATE=${XDG_STATE_HOME:-$HOME/.local/state}/fortnite-on-linux
GAME_DIR=${FOL_GAME_DIR:-$HOME/Documents/Fortnite/++Fortnite+Release-12}
PROTON=${FOL_PROTON:-$DATA/runtimes/experimental-20260924/proton}
ERBIUM=${FOL_ERBIUM_DLL:-$(ls "$REPO"/build-erbium/*/erbium-solo.dll 2>/dev/null | head -1 || true)}
WIN64="$GAME_DIR/FortniteGame/Binaries/Win64"
VERSION=${FOL_VERSION:-$(grep -aoE -m1 '\+\+Fortnite\+Release-[0-9]{1,2}\.[0-9]{1,2}' \
    "$WIN64/FortniteClient-Win64-Shipping.exe" 2>/dev/null | head -1 | sed 's/.*Release-//' || true)}
PROFILE=$REPO/profiles/fortnite/$VERSION.toml
[[ -n $VERSION && -f $PROFILE ]] ||
    { echo "No supported Fortnite build in $GAME_DIR (detected: ${VERSION:-none})" >&2; exit 1; }
export FOL_VERSION=$VERSION
profile() {
    python3 -c 'import sys, tomllib
v = tomllib.load(open(sys.argv[1], "rb")).get(sys.argv[2], "")
sys.stdout.write("".join(x + "\n" for x in v) if isinstance(v, list) else v + "\n")' "$PROFILE" "$1"
}
COMPAT=$DATA/proton-prefixes/$VERSION
LOGS="$COMPAT/pfx/drive_c/users/steamuser/AppData/Local/FortniteGame/Saved/Logs"
winpath() { printf 'Z:%s' "${1//\//\\}"; }
LAWIN_CONFIG=$CONFIG/backend/lawin.json
# Owner-only relay config for shared accounts (integrations/lawin/relay.cjs).
write_remote_config() {
    python3 - "$CONFIG/backend/lawin.json" "$CONFIG/backend/remote.json" \
        "$REPO/integrations/lawin/relay.cjs" "$1" <<'PY'
import json, os, sys
source, target, runner, server = sys.argv[1:5]
data = {"node": json.load(open(source))["node"], "runner": runner, "server": server,
        "username": os.environ["FOL_MP_LOGIN"], "password": os.environ["FOL_MP_PASSWORD"]}
fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
os.fchmod(fd, 0o600)
with os.fdopen(fd, "w") as out:
    json.dump(data, out)
PY
}

if [[ ${1:-} != --inner ]]; then
    export FOL_MATCH_MODE=${1:---both}
    required=("$PROTON" "$REPO/build/windows/fol-inject.exe" "$REPO/build/windows/fol-match-router.dll"
              "$REPO/build/fortnite-backend" "$WIN64/FortniteClient-Win64-Shipping.exe")
    if [[ $FOL_MATCH_MODE != --client ]]; then
        [[ -n $(profile server_login_marker) ]] ||
            { echo "Hosting a game server isn't supported for $VERSION yet; turn off hosting" >&2; exit 1; }
        required+=("$ERBIUM")
    fi
    [[ $FOL_MATCH_MODE != --server && -n $(profile sprint_layout) ]] &&
        required+=("$REPO/build/windows/fol-sprint.dll")
    for f in "${required[@]}"; do
        [[ -n $f && -e $f ]] || { echo "Missing: ${f:-Erbium DLL} (run ./build.sh, helpers/windows/build.sh, Erbium build)" >&2; exit 1; }
    done
    SESSION=$(date -u +%Y-%m-%d_%H-%M-%S)_match
    export FOL_SESSION_ID=$SESSION FOL_MATCH_DIR="$STATE/logs/sessions/$SESSION"
    mkdir -p "$FOL_MATCH_DIR"
    echo "Session logs: $FOL_MATCH_DIR"
    OUTER=$(stat -L -c '%d:%i' /proc/self/ns/net)
    # Network: a loopback-only namespace (user + net only). With multiplayer
    # (FOL_MP_MODE=join|host) pasta is attached to that same namespace for real
    # networking; the inner script then strips routes to the minimum (see
    # apply_multiplayer_routes). Letting pasta create the namespaces instead
    # would also isolate SysV IPC, breaking X11 MIT-SHM (the game exits).
    PASTA=()
    case ${FOL_MP_MODE:-} in
    "") ;;
    join|host)
        command -v pasta >/dev/null ||
            { echo "Multiplayer needs pasta: install the passt package (dnf install passt / pacman -S passt)" >&2; exit 1; }
        # Fedora's SELinux policy confines pasta (pasta_t) to Podman's way of joining
        # namespaces and denies opening /proc/PID/ns. Run it in the caller's own
        # unconfined domain, as on distributions without SELinux. pasta re-executes
        # pasta.avx2 on AVX2 CPUs (entering pasta_t again), so start that one directly.
        PASTA_BIN=(pasta)
        if command -v selinuxenabled >/dev/null && selinuxenabled && command -v runcon >/dev/null &&
            [[ $(id -Z 2>/dev/null) == *:unconfined_t:* ]]; then
            PASTA_BIN=(runcon -t unconfined_t -- "$(command -v pasta)")
            if command -v pasta.avx2 >/dev/null && grep -qw avx2 /proc/cpuinfo; then
                PASTA_BIN[-1]=$(command -v pasta.avx2)
            fi
        fi
        if [[ $FOL_MP_MODE == join ]]; then
            target=${FOL_MP_SERVER:-}
            port=7777
            [[ $target == *:* ]] && { port=${target##*:}; target=${target%:*}; }
            ip=$(getent ahostsv4 "$target" 2>/dev/null | awk 'NR==1 {print $1}')
            [[ $ip =~ ^[0-9]+(\.[0-9]+){3}$ && $port =~ ^[0-9]+$ ]] ||
                { echo "Cannot resolve multiplayer server '${FOL_MP_SERVER:-}'" >&2; exit 1; }
            export FOL_GAME_SERVER="$ip:$port" FOL_MATCH_MODE=--client
            echo "Multiplayer: joining $FOL_GAME_SERVER (no local server)"
            if [[ ${FOL_MP_ACCOUNTS:-own} == server ]]; then
                # Shared accounts: sign in to the server's backend (its admin
                # website manages accounts and cosmetics) through relay.cjs.
                [[ -n ${FOL_MP_LOGIN:-} && -n ${FOL_MP_PASSWORD:-} ]] ||
                    { echo "Using the server's accounts needs the login and password from its admin website" >&2; exit 1; }
                LAWIN_CONFIG=$CONFIG/backend/remote.json
                write_remote_config "$ip" ||
                    { echo "Cannot write $LAWIN_CONFIG" >&2; exit 1; }
                unset FOL_MP_PASSWORD
                echo "Multiplayer: signing in to the server's accounts as $FOL_MP_LOGIN"
            fi
            PASTA=("${PASTA_BIN[@]}" --config-net --quiet --no-map-gw -t none -u none -T none -U none)
        else
            [[ $FOL_MATCH_MODE == --client ]] &&
                { echo "Hosting for others needs the local game server enabled" >&2; exit 1; }
            echo "Multiplayer: hosting on UDP 7777 for LAN/VPN players"
            PASTA=("${PASTA_BIN[@]}" --config-net --quiet --no-map-gw -t none -u 7777 -T none -U none)
        fi ;;
    *) echo "FOL_MP_MODE must be join or host" >&2; exit 1 ;;
    esac
    SESSION_CMD=(env -u OPENSSL_ia32cap STEAM_COMPAT_CLIENT_INSTALL_PATH="$COMPAT/empty-steam-client"
        STEAM_COMPAT_DATA_PATH="$COMPAT" WINEPREFIX="$COMPAT/pfx" SteamAppId=0 SteamGameId=0
        PROTON_LOG=0 WINEDLLOVERRIDES='winemenubuilder.exe=d' NO_PROXY=127.0.0.1,localhost
        no_proxy=127.0.0.1,localhost DXVK_LOG_PATH="$FOL_MATCH_DIR"
        unshare --user --map-root-user --net -- "$REPO/build/fortnite-backend"
        --config "$CONFIG/backend/identity.json" --log "$FOL_MATCH_DIR/backend.log"
        --isolated-session --outer-netns "$OUTER" --lawin-config "$LAWIN_CONFIG"
        run -- /bin/bash "$REPO/scripts/fortnite-match.sh" --inner)
    ((${#PASTA[@]})) || exec "${SESSION_CMD[@]}"
    "${SESSION_CMD[@]}" &
    session_pid=$!
    trap 'kill -TERM $session_pid 2>/dev/null' TERM INT
    for ((i = 0; i < 100; i++)); do # wait until it is inside its own namespace
        ns=$(stat -L -c '%d:%i' /proc/$session_pid/ns/net 2>/dev/null) || break
        [[ $ns != "$OUTER" ]] && break
        sleep 0.1
    done
    "${PASTA[@]}" "$session_pid" 2> >(grep -v nameserver >&2) ||
        { echo "Multiplayer: pasta could not attach" >&2; kill -TERM $session_pid; }
    wait $session_pid
    exit $?
fi

# ---- inside the isolated namespace, after Lawin is ready ----
S=$FOL_MATCH_DIR

# Multiplayer: no default route, so nothing but the allowed peers (and never
# Epic's services) is reachable. Join: the one server. Host: LAN/VPN ranges.
apply_multiplayer_routes() {
    local gw dev net i
    for ((i = 0; i < 50; i++)); do # pasta configures the namespace from outside
        ip -4 route show default | grep -q . && break
        sleep 0.2
    done
    read -r gw dev < <(ip -4 route show default | awk '{print $3, $5; exit}')
    [[ -n $gw ]] || { echo "Multiplayer: no network inside the sandbox" >&2; return 1; }
    ip -4 route del default
    ip -6 route del default 2>/dev/null || true
    if [[ $FOL_MP_MODE == join ]]; then
        ip -4 route get "${FOL_GAME_SERVER%:*}" >/dev/null 2>&1 ||
            ip -4 route add "${FOL_GAME_SERVER%:*}" via "$gw" dev "$dev"
    else
        for net in 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 100.64.0.0/10; do
            ip -4 route add "$net" via "$gw" dev "$dev" 2>/dev/null || true
        done
        echo "Multiplayer: players can join at $(ip -4 -o addr show dev "$dev" scope global |
            awk '{sub(/\/.*/, "", $4); print $4; exit}'):7777 (UDP)"
    fi
}
[[ -n ${FOL_MP_MODE:-} ]] && apply_multiplayer_routes
LOGIN_FILE=$CONFIG/backend/local-login.json
[[ ${FOL_MP_MODE:-} == join && ${FOL_MP_ACCOUNTS:-own} == server ]] && LOGIN_FILE=$CONFIG/backend/remote.json
{ read -r USERNAME; read -r PASSWORD; } < <(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d["username"]); print(d["password"])' "$LOGIN_FILE")
mapfile -t GAME_ARGS < <(profile match_arguments)
GAME_ARGS+=("-AUTH_LOGIN=$USERNAME" "-AUTH_PASSWORD=$PASSWORD" -AUTH_TYPE=epic)
EXE=$(winpath "$WIN64/FortniteClient-Win64-Shipping.exe")
# NVIDIA + XWayland (Hyprland, multi-monitor/high refresh): with the driver's
# explicit-sync path the compositor only shows new frames when the cursor moves.
export __NV_DISABLE_EXPLICIT_SYNC=${__NV_DISABLE_EXPLICIT_SYNC:-1}
export FOL_HTTP_OBSERVER_LAYOUT=$(profile http_observer_layout)
export FOL_HTTP_ROUTER_LAYOUT=$(profile http_router_layout)
export FOL_INJECT_EARLY=$(winpath "$REPO/build/windows/fol-match-router.dll")
# Suspended (never-run) launcher/EAC bootstrap processes next to each game,
# for builds whose environment check otherwise closes the game (12.50).
presence=()
while read -r helper; do presence+=("$(winpath "$WIN64/$helper")"); done < <(profile match_presence)
export FOL_INJECT_PRESENCE=$(IFS=';'; echo "${presence[*]}")
cd "$WIN64"
pids=()
# Hyprland: with per-region damage tracking, fullscreen XWayland games only
# repaint where the cursor moves. Repaint whole monitors while we run.
damage_prev=
SERVER_APP_ID=9999990
if [[ -n ${HYPRLAND_INSTANCE_SIGNATURE:-} ]] && command -v hyprctl >/dev/null; then
    damage_prev=$(hyprctl -j getoption debug:damage_tracking 2>/dev/null |
        python3 -c 'import json,sys; print(json.load(sys.stdin)["int"])' 2>/dev/null || true)
    [[ -n $damage_prev && $damage_prev != 1 ]] &&
        hyprctl eval 'hl.config({debug = {damage_tracking = 1}})' >/dev/null 2>&1
    # Proton names windows steam_app_<SteamGameId>; the server's own ID lets
    # this rule keep its windows (shown for seconds while it loads, before
    # the guard can hide them) on a hidden special workspace.
    hyprctl eval "hl.window_rule({ name = \"project_tux_server\", match = { class = \"^steam_app_$SERVER_APP_ID\$\" }, workspace = \"special:tux_server silent\", no_initial_focus = true, no_focus = true, no_anim = true })" >/dev/null 2>&1 || true
    # Wine lists fullscreen resolutions of the X11 primary output only; without
    # one it uses the first (possibly a smaller monitor). Use the focused one.
    focused=$(hyprctl -j monitors 2>/dev/null |
        python3 -c 'import json,sys; print(next((m["name"] for m in json.load(sys.stdin) if m["focused"]),""))' 2>/dev/null || true)
    [[ -n $focused ]] && command -v xrandr >/dev/null && xrandr --output "$focused" --primary >/dev/null 2>&1 || true
fi
cleanup() {
    [[ -n $damage_prev && $damage_prev != 1 ]] &&
        hyprctl eval "hl.config({debug = {damage_tracking = $damage_prev}})" >/dev/null 2>&1
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
    WINEPREFIX="$COMPAT/pfx" "$(dirname "$PROTON")/files/bin/wineserver" -k 2>/dev/null || true
    if [[ $FOL_MATCH_MODE == --client ]]; then
        cp -f "$LOGS"/FortniteGame.log "$S/client-game.log" 2>/dev/null || true
    else
        cp -f "$LOGS"/FortniteGame.log "$S/server-game.log" 2>/dev/null || true
        cp -f "$LOGS"/FortniteGame_2.log "$S/client-game.log" 2>/dev/null || true
    fi
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
    # Find the client by window class (the server uses its own); PIDs differ
    # inside pasta's PID namespace.
    local address
    local address fullscreen
    read -r address fullscreen < <(hyprctl -j clients | python3 -c 'import json,sys; c=next((c for c in json.load(sys.stdin) if c["class"]=="steam_app_0"),None); print(c["address"], c["fullscreen"]) if c else print("", 0)')
    hyprctl dispatch focuswindow "address:$address" >/dev/null
    # The input helper needs a 16:9 client; fullscreen unless already.
    [[ $fullscreen == 0 ]] && hyprctl dispatch fullscreen 0 >/dev/null
    sleep 5
    if ! grep -qs 'UI.State.Athena.Frontend' "$log"; then
        shot autoplay-1-subgame; ui select-br; sleep 3; ui enter-br; sleep 15
    fi
    shot autoplay-2-lobby
    for i in 1 2 3; do ui match-play && grep -qs 'LogNet: Browse: [0-9]' "$log" && break; sleep 10; done
    sleep 20
    shot autoplay-3-after-play; sleep 60; shot autoplay-4-match; sleep 60; shot autoplay-5-match
}

udp_bound() { grep -q ':1E61 ' /proc/net/udp /proc/net/udp6 2>/dev/null; }

# One game server process, in the foreground. $1 numbers its logs.
run_server() {
    FOL_HTTP_MODULE_REPORT="$(winpath "$S")\\router-server-$1.json" \
    FOL_INJECT_EARLY="$FOL_INJECT_EARLY;$(winpath "$REPO/build/windows/fol-server-guard.dll")" \
    FOL_INJECT_STATUS="$(winpath "$S")\\inject-server-$1.txt" FOL_INJECT_LATE=$(winpath "$ERBIUM") \
    FOL_ERBIUM_LOG="$(winpath "$S")\\erbium-$1.log" \
    SteamGameId=$SERVER_APP_ID SteamAppId=$SERVER_APP_ID \
    FOL_INJECT_LOG='C:\users\steamuser\AppData\Local\FortniteGame\Saved\Logs\FortniteGame.log' \
    FOL_INJECT_MARKER=$(profile server_login_marker) \
    WINEDEBUG='+timestamp,+pid,+seh,+loaddll' \
        "$PROTON" run "$REPO/build/windows/fol-inject.exe" "$EXE" -log -nullrhi -nosplash -nosound \
        -RenderOffScreen -windowed -ResX=640 -ResY=360 \
        "${GAME_ARGS[@]}" >"$S/server-wine-$1.log" 2>&1
}
# Erbium exits after a finished match (bAutoRestart); start a fresh server each
# time so Play works again. The matchmaker queues players until it listens.
kill_tree() {
    local child
    for child in $(pgrep -P "$1" 2>/dev/null); do kill_tree "$child"; done
    kill -TERM "$1" 2>/dev/null
}
server_supervisor() {
    set +e
    local n=1 failures=0 pid code started i status
    while :; do
        # Fresh log: the injector waits for this server's own sign-in line.
        rm -f "$LOGS/FortniteGame.log"
        status="$S/inject-server-$n.txt"
        started=$SECONDS
        run_server $n &
        pid=$!
        for ((i = 0; i < 600; i++)); do
            if udp_bound; then
                echo "Game server listening on 127.0.0.1:7777 after ${i}s"
                break
            fi
            grep -qs "child exited" "$status" && break
            kill -0 $pid 2>/dev/null || break
            sleep 1
        done
        # `proton run` only returns once every Wine process in the prefix has
        # ended (including the client), so watch the server game itself.
        until grep -qs "child exited" "$status" || ! kill -0 $pid 2>/dev/null; do sleep 2; done
        code=$(sed -n 's/.*child exited \([0-9]*\).*/\1/p' "$status" 2>/dev/null | tail -1)
        kill_tree $pid
        wait $pid 2>/dev/null
        cp -f "$LOGS/FortniteGame.log" "$S/server-game-$n.log" 2>/dev/null
        if ((SECONDS - started < 90)); then failures=$((failures + 1)); else failures=0; fi
        if ((failures >= 3)); then
            echo "Game server keeps failing to start; see $S"
            return 1
        fi
        echo "Game server exited (${code:-?}); starting a new one for the next match..."
        n=$((n + 1))
        sleep 2
    done
}

if [[ $FOL_MATCH_MODE != --client ]]; then
    mkdir -p "$LOGS"
    rm -f "$LOGS/FortniteGame_2.log"
    echo "Starting headless game server..."
    server_supervisor &
    pids+=($!)
    for ((i = 0; i < 600; i++)); do
        udp_bound && break
        kill -0 "${pids[0]}" 2>/dev/null || { echo "Game server exited early; see $S" >&2; exit 1; }
        sleep 1
    done
    udp_bound || { echo "Game server never listened on 7777; see $S" >&2; exit 1; }
fi

if [[ $FOL_MATCH_MODE == --server ]]; then
    wait "${pids[0]}"
    exit $?
fi
[[ $FOL_MATCH_MODE == --client ]] && rm -f "$LOGS/FortniteGame.log"
echo "Starting client..."
server_pid=$(pgrep -f 'Win64-Shipping.exe' | head -1 || true)
# Sprint by default for builds without the game's own option (3.1); the
# player's game only, never the server.
CLIENT_EARLY=$FOL_INJECT_EARLY
SPRINT_LAYOUT=$(profile sprint_layout)
[[ -n $SPRINT_LAYOUT ]] && CLIENT_EARLY="$CLIENT_EARLY;$(winpath "$REPO/build/windows/fol-sprint.dll")"
FOL_HTTP_MODULE_REPORT="$(winpath "$S")\\router-client.json" WINEDEBUG='-all' \
FOL_INJECT_STATUS="$(winpath "$S")\\inject-client.txt" \
FOL_INJECT_EARLY="$CLIENT_EARLY" FOL_SPRINT_LAYOUT="$SPRINT_LAYOUT" \
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
