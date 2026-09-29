#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
for tool in clang lld-link cmake; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
test -f /usr/include/wine/windows/windows.h
test -f /usr/lib/wine/x86_64-windows/libkernel32.a
mkdir -p build/windows
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I/usr/include/wine/windows -I/usr/include/wine/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_probe.c -o build/windows/http_probe.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-probe.exe build/windows/http_probe.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
file build/windows/fol-http-probe.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_REQUEST_OBSERVER -I/usr/include/wine/windows -I/usr/include/wine/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_probe.c -o build/windows/http_observer.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-observer.exe build/windows/http_observer.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
file build/windows/fol-http-observer.exe
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -DFOL_MODULE_BOOTSTRAP -I/usr/include/wine/windows -I/usr/include/wine/msvcrt \
    -Wall -Wextra -Werror -O1 -c helpers/windows/http_probe.c -o build/windows/module_bootstrap.obj
lld-link /entry:probe_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-module-bootstrap.exe build/windows/module_bootstrap.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I/usr/include/wine/windows -I/usr/include/wine/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_module.c -o build/windows/http_module.obj
lld-link /dll /entry:module_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-adapter.dll build/windows/http_module.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
cmake -DARTIFACT="$PWD/build/windows/fol-http-adapter.dll" -P helpers/windows/module_manifest.cmake
file build/windows/fol-module-bootstrap.exe build/windows/fol-http-adapter.dll
clang --target=x86_64-w64-windows-gnu -ffreestanding -fno-stack-protector -fno-builtin -funwind-tables \
    -I/usr/include/wine/windows -I/usr/include/wine/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_fixture.c -o build/windows/http_fixture.obj
lld-link /entry:fixture_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-fixture.exe build/windows/http_fixture.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
file build/windows/fol-http-fixture.exe
