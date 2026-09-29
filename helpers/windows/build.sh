#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
for tool in clang lld-link; do
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
    -I/usr/include/wine/windows -I/usr/include/wine/msvcrt -Wall -Wextra -Werror -O1 \
    -c helpers/windows/http_fixture.c -o build/windows/http_fixture.obj
lld-link /entry:fixture_entry /subsystem:console /nodefaultlib /machine:x64 /timestamp:0 \
    /out:build/windows/fol-http-fixture.exe build/windows/http_fixture.obj \
    /usr/lib/wine/x86_64-windows/libkernel32.a
file build/windows/fol-http-fixture.exe
