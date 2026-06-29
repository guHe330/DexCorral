#include "Registration.h"
#include "Guids.h"
#include <shlobj.h>
#include <strsafe.h>
#include <stdio.h>

// CLSID as string: {7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}
static const wchar_t* CLSID_STRING = L"{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}";
static const wchar_t* EXTENSION_NAME = L"DexCorral";

static HRESULT SetRegistryValue(HKEY hKeyRoot, const wchar_t* subKey,
    const wchar_t* valueName, const wchar_t* data) {
    HKEY hKey;
    LONG result = RegCreateKeyExW(hKeyRoot, subKey, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
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
    LONG result = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);

    result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    if (result == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(result);
}

HRESULT RegisterShellExtension(const wchar_t* dllPath) {
    HRESULT hr;
    wchar_t keyPath[512];

    // 1. Register CLSID → InprocServer32
    StringCchPrintfW(keyPath, 512, L"CLSID\\%s", CLSID_STRING);
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, keyPath, nullptr, EXTENSION_NAME);
    if (FAILED(hr)) return hr;

    StringCchPrintfW(keyPath, 512, L"CLSID\\%s\\InprocServer32", CLSID_STRING);
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, keyPath, nullptr, dllPath);
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, keyPath, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    // 2. Register SharedTaskScheduler (auto-load into Explorer on startup)
    // Clean up stray subkey left by older installs that used the wrong format.
    StringCchPrintfW(keyPath, 512,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler\\%s",
        CLSID_STRING);
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, keyPath);
    // Explorer calls CoCreateInstance on every CLSID listed here during startup,
    // loading the DLL in-process. ShellServiceObjectDelayLoad is no longer honored
    // for third-party DLLs on Windows 10/11.
    // Structure: value name = CLSID, value data = display name (same as Fences).
    hr = SetRegistryValue(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler",
        CLSID_STRING, EXTENSION_NAME);
    if (FAILED(hr)) return hr;

    // 3. Register icon overlay handler (Explorer loads ALL overlay handlers on startup)
    // Prefixed with space so it sorts early (Explorer only loads first 15 alphabetically)
    hr = SetRegistryValue(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ DexCorral",
        nullptr, CLSID_STRING);
    if (FAILED(hr)) return hr;

    // 4. Register context menu handler for desktop background right-click
    hr = SetRegistryValue(HKEY_CLASSES_ROOT,
        L"Directory\\Background\\ShellEx\\ContextMenuHandlers\\DexCorral",
        nullptr, CLSID_STRING);
    if (FAILED(hr)) return hr;

    // 5. Notify Explorer of the change
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

HRESULT UnregisterShellExtension() {
    wchar_t keyPath[512];

    // Remove icon overlay handler
    DeleteRegistryKey(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ DexCorral");

    // Remove context menu handler
    DeleteRegistryKey(HKEY_CLASSES_ROOT,
        L"Directory\\Background\\ShellEx\\ContextMenuHandlers\\DexCorral");

    // Remove SharedTaskScheduler entries (both the correct value and any stray subkey)
    DeleteRegistryValue(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler",
        CLSID_STRING);
    StringCchPrintfW(keyPath, 512,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler\\%s",
        CLSID_STRING);
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, keyPath);

    // Remove ShellServiceObjectDelayLoad entry (written by older installs)
    DeleteRegistryValue(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellServiceObjectDelayLoad",
        L"DexCorral");

    // Remove CLSID
    StringCchPrintfW(keyPath, 512, L"CLSID\\%s", CLSID_STRING);
    DeleteRegistryKey(HKEY_CLASSES_ROOT, keyPath);

    // Notify Explorer
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}
