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

/// Why the hook was deliberately not installed at start ("standby")
enum class HookStandbyReason
{
    None = 0,       /// Not in standby: the hook is, or will be, installed
    CrashGuard = 1, /// Explorer died repeatedly with the hook installed
    Conflict = 2    /// An incompatible program manages the desktop icon view
};

/**
 * Returns why the hook was not installed, or None. Decided once, by the first
 * InitializeCorralHook call, which happens before the app thread starts. In
 * standby the app shows no corrals and hides no icons, and says so in the tray.
 */
HookStandbyReason GetCorralHookStandbyReason();

/// For a Conflict standby: the product to name in messages, e.g. "Stardock Fences 6.5.4.1".
/// Empty otherwise. Valid for the lifetime of the process.
const wchar_t* GetCorralHookConflictProduct();

/// True if crash-guard standby began with this start, rather than being carried
/// over from an earlier one. The app shows its explanation dialog only then.
bool WasCorralHookStandbyEnteredNow();

/**
 * Leaves crash-guard standby on the user's request: forgets the recorded
 * failures and installs the hook now. Returns true if the hook is active
 * afterwards. Does nothing for a Conflict standby, which ends by itself at a
 * start where the conflict is gone.
 */
bool ResumeCorralHook();
