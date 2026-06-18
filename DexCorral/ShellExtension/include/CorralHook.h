/**
 * CorralHook.h - Explorer.exe hook API
 *
 * Public API for the CorralHook.dll which is injected into explorer.exe.
 * Provides functions to initialize/cleanup the hook and query its status.
 */

#pragma once
#include <Windows.h>

/**
 * Initializes the Explorer hook DLL.
 * Subclasses SHELLDLL_DefView and SysListView32 to intercept NM_CUSTOMDRAW messages
 * and use CDRF_SKIPDEFAULT to hide icons owned by corrals.
 * Returns true if successful.
 */
bool InitializeCorralHook();

/**
 * Cleans up the hook and restores original window procedures.
 * Called during DLL_PROCESS_DETACH or when the hook should be disabled.
 */
void CleanupCorralHook();

/**
 * Returns true if the hook is currently active and subclassing the desktop ListView.
 */
bool IsCorralHookActive();

/**
 * Returns true if the hook was deliberately not installed this session because
 * Explorer died repeatedly while the hook was active (safe mode). The app
 * should inform the user via a tray notice. The failure counter is reset on
 * entering safe mode, so the next session attempts the hook again.
 */
bool IsCorralHookSafeMode();
