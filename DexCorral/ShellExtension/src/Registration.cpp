#include "Registration.h"
#include "Guids.h"
#include "Version.h"
#include <shlobj.h>
#include <strsafe.h>
#include <stdio.h>

// CLSID as string: {7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}
// The same CLSID is used in both scopes on purpose. HKCR is a merge of
// HKLM\Software\Classes and HKCU\Software\Classes with HKCU winning, so two
// installs can never both be live: the per-user one silently shadows the
// machine one. The installer refuses that combination rather than allow it.
static const wchar_t* CLSID_STRING = L"{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}";
static const wchar_t* EXTENSION_NAME = L"DexCorral";

// Everything below is written under Software\Classes of the scope's root, never
// through HKEY_CLASSES_ROOT: an HKCR write lands in HKLM if the key already
// exists there and in HKCU otherwise, which is exactly the ambiguity the two
// scopes exist to remove.
static const wchar_t* SUBKEY_CLASSES     = L"Software\\Classes";
static const wchar_t* SUBKEY_CONTEXTMENU = L"Software\\Classes\\Directory\\Background\\ShellEx\\ContextMenuHandlers\\DexCorral";
static const wchar_t* SUBKEY_MARKER      = L"Software\\DexCorral";

// Machine-scope-only load vectors. Explorer reads both from HKLM only; there is
// no HKCU equivalent, so a user-scope install relies on the Run key injection
// (DexCorral.exe --startup) and the context menu handler instead.
static const wchar_t* SUBKEY_STS     = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler";
static const wchar_t* SUBKEY_OVERLAY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ DexCorral";
static const wchar_t* SUBKEY_SSODL   = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellServiceObjectDelayLoad";

// DexCorral is x64-only, but be explicit so a 32-bit host cannot land in the
// WOW6432Node view and register a CLSID Explorer never reads.
static const REGSAM kView = KEY_WOW64_64KEY;

static HRESULT SetRegistryValue(HKEY hKeyRoot, const wchar_t* subKey,
    const wchar_t* valueName, const wchar_t* data) {
    HKEY hKey;
    LONG result = RegCreateKeyExW(hKeyRoot, subKey, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | kView, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(result);

    result = RegSetValueExW(hKey, valueName, 0, REG_SZ,
        (const BYTE*)data, (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(result);
}

static HRESULT DeleteRegistryKey(HKEY hKeyRoot, const wchar_t* subKey) {
    LONG result = RegDeleteTreeW(hKeyRoot, subKey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;  // Already gone
    return HRESULT_FROM_WIN32(result);
}

static HRESULT DeleteRegistryValue(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_SET_VALUE | kView, &hKey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);

    result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(result);
}

HKEY ScopeRoot(InstallScope scope) {
    return scope == InstallScope::Machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

// True if path sits inside a known folder (case-insensitive, folder boundary).
static bool IsUnderFolder(const wchar_t* path, REFKNOWNFOLDERID folder) {
    PWSTR dir = nullptr;
    if (FAILED(SHGetKnownFolderPath(folder, 0, nullptr, &dir)) || !dir)
        return false;

    const size_t len = wcslen(dir);
    const bool under = len > 0
        && _wcsnicmp(path, dir, len) == 0
        && (path[len] == L'\\' || path[len] == L'\0');
    CoTaskMemFree(dir);
    return under;
}

InstallScope InferInstallScope(const wchar_t* modulePath) {
    if (modulePath && (IsUnderFolder(modulePath, FOLDERID_ProgramFiles) ||
                       IsUnderFolder(modulePath, FOLDERID_ProgramFilesX86)))
        return InstallScope::Machine;
    return InstallScope::User;
}

// Records where this install lives so the installer can detect the other scope
// and refuse a conflicting install. Written on register, removed on unregister.
// Only these three values are touched: Software\DexCorral also holds Language
// and the hook's runtime HookStartPending/HookFailureCount, which must survive.
static HRESULT WriteInstallMarker(HKEY root, const wchar_t* dllPath, InstallScope scope) {
    wchar_t dir[MAX_PATH];
    StringCchCopyW(dir, MAX_PATH, dllPath);
    wchar_t* lastSlash = wcsrchr(dir, L'\\');
    if (lastSlash) *lastSlash = 0;

    HRESULT hr = SetRegistryValue(root, SUBKEY_MARKER, L"InstallDir", dir);
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(root, SUBKEY_MARKER, L"InstallScope",
        scope == InstallScope::Machine ? L"machine" : L"user");
    if (FAILED(hr)) return hr;
    return SetRegistryValue(root, SUBKEY_MARKER, L"Version", DEXCORRAL_VERSION);
}

static void DeleteInstallMarker(HKEY root) {
    DeleteRegistryValue(root, SUBKEY_MARKER, L"InstallDir");
    DeleteRegistryValue(root, SUBKEY_MARKER, L"InstallScope");
    DeleteRegistryValue(root, SUBKEY_MARKER, L"Version");
}

HRESULT RegisterShellExtension(const wchar_t* dllPath, InstallScope scope) {
    HRESULT hr;
    wchar_t keyPath[512];
    const HKEY root = ScopeRoot(scope);

    // 1. CLSID -> InprocServer32
    StringCchPrintfW(keyPath, 512, L"%s\\CLSID\\%s", SUBKEY_CLASSES, CLSID_STRING);
    hr = SetRegistryValue(root, keyPath, nullptr, EXTENSION_NAME);
    if (FAILED(hr)) return hr;

    StringCchPrintfW(keyPath, 512, L"%s\\CLSID\\%s\\InprocServer32", SUBKEY_CLASSES, CLSID_STRING);
    hr = SetRegistryValue(root, keyPath, nullptr, dllPath);
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(root, keyPath, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    // 2. Context menu handler for the desktop background right-click.
    //    A real feature, not a load vector, so both scopes register it.
    hr = SetRegistryValue(root, SUBKEY_CONTEXTMENU, nullptr, CLSID_STRING);
    if (FAILED(hr)) return hr;

    // 3. Machine scope only: the two auto-load vectors Explorer reads from HKLM.
    if (scope == InstallScope::Machine) {
        // Clean up a stray subkey left by older installs that used the wrong format.
        StringCchPrintfW(keyPath, 512, L"%s\\%s", SUBKEY_STS, CLSID_STRING);
        DeleteRegistryKey(HKEY_LOCAL_MACHINE, keyPath);
        // Explorer calls CoCreateInstance on every CLSID listed here during startup,
        // loading the DLL in-process. ShellServiceObjectDelayLoad is no longer honored
        // for third-party DLLs on Windows 11.
        // Structure: value name = CLSID, value data = display name.
        hr = SetRegistryValue(HKEY_LOCAL_MACHINE, SUBKEY_STS, CLSID_STRING, EXTENSION_NAME);
        if (FAILED(hr)) return hr;

        // Icon overlay handler: Explorer loads ALL overlay handlers on startup.
        // Prefixed with a space so it sorts early (Explorer only loads the first 15).
        hr = SetRegistryValue(HKEY_LOCAL_MACHINE, SUBKEY_OVERLAY, nullptr, CLSID_STRING);
        if (FAILED(hr)) return hr;
    }

    // 4. Install marker, for cross-scope conflict detection by the installer
    hr = WriteInstallMarker(root, dllPath, scope);
    if (FAILED(hr)) return hr;

    // 5. Notify Explorer of the change
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

HRESULT UnregisterShellExtension(InstallScope scope) {
    wchar_t keyPath[512];
    const HKEY root = ScopeRoot(scope);

    // Context menu handler
    DeleteRegistryKey(root, SUBKEY_CONTEXTMENU);

    // Machine-only load vectors. A user-scope uninstall must leave these alone:
    // they may belong to a machine-wide install that other users still rely on.
    if (scope == InstallScope::Machine) {
        DeleteRegistryKey(HKEY_LOCAL_MACHINE, SUBKEY_OVERLAY);
        DeleteRegistryValue(HKEY_LOCAL_MACHINE, SUBKEY_STS, CLSID_STRING);
        StringCchPrintfW(keyPath, 512, L"%s\\%s", SUBKEY_STS, CLSID_STRING);
        DeleteRegistryKey(HKEY_LOCAL_MACHINE, keyPath);
    }

    // CLSID
    StringCchPrintfW(keyPath, 512, L"%s\\CLSID\\%s", SUBKEY_CLASSES, CLSID_STRING);
    DeleteRegistryKey(root, keyPath);

    DeleteInstallMarker(root);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

HRESULT CleanupLegacyRegistration() {
    wchar_t keyPath[512];

    // Pre-1.0.28 wrote the CLSID and context menu handler through HKEY_CLASSES_ROOT,
    // which resolves to HKLM\Software\Classes for an elevated install. Deleting via
    // HKCR covers that and any copy an unelevated run left in HKCU.
    StringCchPrintfW(keyPath, 512, L"CLSID\\%s", CLSID_STRING);
    DeleteRegistryKey(HKEY_CLASSES_ROOT, keyPath);
    DeleteRegistryKey(HKEY_CLASSES_ROOT,
        L"Directory\\Background\\ShellEx\\ContextMenuHandlers\\DexCorral");

    // Load vectors, plus the ShellServiceObjectDelayLoad entry from even older installs
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, SUBKEY_OVERLAY);
    DeleteRegistryValue(HKEY_LOCAL_MACHINE, SUBKEY_STS, CLSID_STRING);
    StringCchPrintfW(keyPath, 512, L"%s\\%s", SUBKEY_STS, CLSID_STRING);
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, keyPath);
    DeleteRegistryValue(HKEY_LOCAL_MACHINE, SUBKEY_SSODL, L"DexCorral");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}
