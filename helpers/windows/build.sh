#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
for tool in clang lld-link cmake file; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
# Wine's Windows headers and x86_64 import libraries. Layouts differ by
# distribution: Arch/CachyOS /usr/lib/wine, Fedora /usr/lib64/wine-wow64/wine
# (or /usr/lib64/wine), Debian/Ubuntu /usr/lib/x86_64-linux-gnu/wine.
# Override with FOL_WINE_INCLUDE / FOL_WINE_LIB.
WINE_INCLUDE=${FOL_WINE_INCLUDE:-/usr/include/wine}
WINE_LIB=${FOL_WINE_LIB:-}
if [[ -z $WINE_LIB ]]; then
    for dir in /usr/lib/wine/x86_64-windows /usr/lib64/wine-wow64/wine/x86_64-windows \
               /usr/lib64/wine/x86_64-windows /usr/lib/x86_64-linux-gnu/wine/x86_64-windows; do
        [[ -f $dir/libkernel32.a ]] && { WINE_LIB=$dir; break; }
    done
fi
[[ -f $WINE_INCLUDE/windows/windows.h ]] ||
    { echo "Missing Wine headers in $WINE_INCLUDE (Arch: wine, Fedora: wine-devel)" >&2; exit 1; }
[[ -n $WINE_LIB && -f $WINE_LIB/libkernel32.a ]] ||
    { echo "Missing Wine x86_64-windows import libraries (Arch: wine, Fedora: wine-devel); set FOL_WINE_LIB" >&2; exit 1; }
mkdir -p build/windows
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_probe.c -o build/windows/http_probe.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-probe.exe build/windows/http_probe.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-http-probe.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_REQUEST_OBSERVER -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_probe.c -o build/windows/http_observer.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-observer.exe build/windows/http_observer.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-http-observer.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_MODULE_BOOTSTRAP -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_probe.c -o build/windows/module_bootstrap.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-module-bootstrap.exe build/windows/module_bootstrap.obj \
    $WINE_LIB/libkernel32.a
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_module.c -o build/windows/http_module.obj
lld-link /dll /entry:module_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-adapter.dll build/windows/http_module.obj \
    $WINE_LIB/libkernel32.a
cmake -DARTIFACT="$PWD/build/windows/fol-http-adapter.dll" -P helpers/windows/module_manifest.cmake
file build/windows/fol-module-bootstrap.exe build/windows/fol-http-adapter.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_INPROCESS_OBSERVER -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_module.c -o build/windows/request_module.obj
lld-link /dll /entry:module_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-request-observer.dll build/windows/request_module.obj \
    $WINE_LIB/libkernel32.a
cmake -DARTIFACT="$PWD/build/windows/fol-http-request-observer.dll" \
    -DMODULE_ROLE=http-request-observer -P helpers/windows/module_manifest.cmake
file build/windows/fol-http-request-observer.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_INPROCESS_OBSERVER -DFOL_LOCAL_ROUTER -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_module.c -o build/windows/router_module.obj
lld-link /dll /entry:module_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-local-router.dll build/windows/router_module.obj \
    $WINE_LIB/libkernel32.a
cmake -DARTIFACT="$PWD/build/windows/fol-http-local-router.dll" \
    -DMODULE_ROLE=http-local-router -P helpers/windows/module_manifest.cmake
file build/windows/fol-http-local-router.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_fixture.c -o build/windows/http_fixture.obj
lld-link /entry:fixture_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-fixture.exe build/windows/http_fixture.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-http-fixture.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/ui_input_probe.c -o build/windows/ui_input_probe.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-ui-input-probe.exe build/windows/ui_input_probe.obj \
    $WINE_LIB/libkernel32.a $WINE_LIB/libuser32.a
file build/windows/fol-ui-input-probe.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_INPROCESS_OBSERVER -DFOL_LOCAL_ROUTER -DFOL_PERSISTENT_ROUTER \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_module.c -o build/windows/match_router.obj
lld-link /dll /entry:module_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-match-router.dll build/windows/match_router.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-match-router.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/fol_inject.c -o build/windows/fol_inject.obj
lld-link /entry:inject_entry /subsystem:windows /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-inject.exe build/windows/fol_inject.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-inject.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/server_guard.c -o build/windows/server_guard.obj
lld-link /dll /entry:guard_entry /subsystem:windows /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-server-guard.dll build/windows/server_guard.obj \
    $WINE_LIB/libkernel32.a $WINE_LIB/libuser32.a
file build/windows/fol-server-guard.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I$WINE_INCLUDE/windows -I$WINE_INCLUDE/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/sprint_default.c -o build/windows/sprint_default.obj
lld-link /dll /entry:sprint_entry /subsystem:windows /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-sprint.dll build/windows/sprint_default.obj \
    $WINE_LIB/libkernel32.a
file build/windows/fol-sprint.dll