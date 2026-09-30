// SPDX-License-Identifier: GPL-3.0-only
// Original compatibility probe using the pinned Erbium SDK headers only.
// No Erbium server entry point, hooks, authentication changes or code patches.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Includes.h"

static HANDLE report = INVALID_HANDLE_VALUE;
static char session[128];

static bool record(const char* event, unsigned value) {
    char line[512];
    const int length = std::snprintf(line, sizeof(line),
        "{\"event\":\"%s\",\"session_id\":\"%s\",\"process_id\":%lu,\"value\":%u}\n",
        event, session, GetCurrentProcessId(), value);
    DWORD written = 0;
    return length > 0 && length < sizeof(line) &&
        WriteFile(report, line, length, &written, nullptr) && written == length &&
        FlushFileBuffers(report);
}

static bool inspect_sdk() {
    if (!record("sdk-init-started", 1)) return false;
    SDK::Init();
    if (!record("sdk-init-completed", 1)) return false;
    const auto version = static_cast<unsigned>(SDK::VersionInfo.FortniteVersion * 100 + 0.5);
    const auto engine = static_cast<unsigned>(SDK::VersionInfo.EngineVersion * 100 + 0.5);
    if (!record("fortnite-version-times100", version) ||
        !record("engine-version-times100", engine)) return false;
    if (version != 1250 || engine != 425) return false;
    if (!SDK::Offsets::GObjectsChunked || !SDK::Offsets::FNameConstructor ||
        !SDK::Offsets::AppendString) return false;
    const auto count = SDK::TUObjectArray::Num();
    if (!record("object-count", count) || count < 1 || count > 2000000) return false;
    const auto mode = SDK::FindClass("FortGameModeAthena");
    const auto driver = SDK::FindClass("IpNetDriver");
    if (!record("game-mode-class-found", mode != nullptr) ||
        !record("net-driver-class-found", driver != nullptr)) return false;
    // UObject::GetFunction starts at the object's Class. A UClass object's Class
    // is the metaclass, not FortGameModeAthena; use its already-existing CDO.
    const auto defaultMode = mode ? mode->GetDefaultObj() : nullptr;
    const auto ready = defaultMode ? defaultMode->GetFunction("ReadyToStartMatch") : nullptr;
    if (!record("ready-to-start-function-found", ready != nullptr) ||
        !record("process-event-vtable-slot", SDK::Offsets::ProcessEventVft)) return false;
    const auto gameEngine = SDK::UEngine::GetEngine();
    const auto viewport = gameEngine ? gameEngine->GameViewport : nullptr;
    const auto world = viewport ? viewport->World : nullptr;
    if (!record("world-found", world != nullptr)) return false;
    // Reflection observations do not establish listening, admission or gameplay.
    return mode && driver && ready && world && SDK::Offsets::ProcessEventVft > 0;
}

static bool guarded_cpp_probe() {
    try { return inspect_sdk(); }
    catch (const std::exception&) { record("cpp-exception", 1); return false; }
    catch (...) { record("unknown-cpp-exception", 1); return false; }
}

static DWORD WINAPI worker(void*) {
    wchar_t path[4096], identity[128], executable[4096];
    const auto size = GetEnvironmentVariableW(L"FOL_SOLO_SDK_REPORT", path, 4096);
    const auto idSize = GetEnvironmentVariableW(L"FOL_SESSION_ID", identity, 128);
    if (!size || size >= 4096 || !idSize || idSize >= 128) return 64;
    for (DWORD i = 0; i < idSize; ++i) {
        const auto c = identity[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'-' || c == L'_')) return 64;
        session[i] = static_cast<char>(c);
    }
    const DWORD executableSize = GetModuleFileNameW(nullptr, executable, 4096);
    if (!executableSize || executableSize >= 4096) return 65;
    const auto name = wcsrchr(executable, L'\\');
    if (!name || wcscmp(name + 1, L"FortniteClient-Win64-Shipping.exe") != 0) return 65;
    report = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (report == INVALID_HANDLE_VALUE) return 68;
    bool successful = false;
    if (record("started", 1)) {
        // Leave startup and local login unchanged. Inspect after ordinary engine
        // initialization; never suppress a game exit if prerequisites are absent.
        Sleep(30000);
        __try { successful = guarded_cpp_probe(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { record("structured-exception", GetExceptionCode()); }
    }
    const bool complete = record("completed", successful ? 1 : 0);
    CloseHandle(report);
    report = INVALID_HANDLE_VALUE;
    return complete && successful ? 0 : 67;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
