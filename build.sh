#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
for tool in cmake ninja c++; do
    command -v "$tool" >/dev/null || { echo "Missing developer tool: $tool" >&2; exit 1; }
done
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel "${FOL_BUILD_JOBS:-4}"
ctest --test-dir build --output-on-failure
