#pragma once
#include <Windows.h>

// Where a DexCorral install lives. The two modes are mutually exclusive on a
// machine (see installer/INSTALLER_NOTES.md); they share one CLSID, so a
// per-user registration would otherwise silently shadow a machine-wide one.
enum class InstallScope {
    User,     // HKCU only, no admin rights needed, this user only
    Machine   // HKLM, admin rights required, every user on the PC
};

// Registry root for a scope's Software\Classes and Software\DexCorral keys.
HKEY ScopeRoot(InstallScope scope);

// Guess the scope from where this DLL sits: under %ProgramFiles% means machine.
// Used when no --scope was given on the command line.
InstallScope InferInstallScope(const wchar_t* modulePath);

HRESULT RegisterShellExtension(const wchar_t* dllPath, InstallScope scope);
HRESULT UnregisterShellExtension(InstallScope scope);

// Removes the pre-1.0.28 layout, which wrote through HKEY_CLASSES_ROOT and had
// no notion of scope. Needs admin; a user-scope uninstall must not call it.
HRESULT CleanupLegacyRegistration();
