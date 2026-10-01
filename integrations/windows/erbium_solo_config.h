// SPDX-License-Identifier: GPL-3.0-only
// Replaces Erbium/Public/Configuration.h in the generated build copy only.
// Ordinary 12.50 BR Solo: no cheats, no siphon, no GUI and
// console output (no D3D11 GUI window) for diagnostics.
#pragma once

struct FConfiguration
{
    static inline auto Playlist = L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    static inline auto MaxTickRate = 30;
    static inline auto bLateGame = false;
    static inline auto LateGameZone = 3;
    static inline auto bLateGameLongZone = false;
    static inline auto bEnableCheats = false;
    static inline auto SiphonAmount = 0;
    static inline auto bInfiniteMats = false;
    static inline auto bInfiniteAmmo = false;
    static inline auto bForceRespawns = false;
    static inline auto bJoinInProgress = false;
    static inline auto bAutoRestart = false;
    static inline auto bKeepInventory = false;
    static inline auto Port = 7777;
    static inline auto bEnableIris = true;
    static inline constexpr auto bGUI = false;
    static inline constexpr auto bCustomCrashReporter = true;
    static inline constexpr auto bUseStdoutLog = true;
    static inline constexpr auto WebhookURL = "";
};
