#include "DesktopIcons.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <Shlwapi.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "Shlwapi.lib")

HWND DesktopIcons::GetDesktopListView() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND shellDll = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);

    if (shellDll == nullptr) {
        // Try WorkerW windows
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            HWND shellDll = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
            if (shellDll != nullptr) {
                *(HWND*)lParam = shellDll;
                return FALSE;
            }
            return TRUE;
        }, (LPARAM)&shellDll);
    }

    if (shellDll == nullptr) return nullptr;
    return FindWindowExW(shellDll, nullptr, L"SysListView32", nullptr);
}

std::wstring DesktopIcons::GetItemText(HWND hListView, HANDLE hProcess, LPVOID pRemoteItem, LPVOID pRemoteText, int index) {
    LVITEMW lvItem = {};
    lvItem.mask = LVIF_TEXT;
    lvItem.iItem = index;
    lvItem.iSubItem = 0;
    lvItem.pszText = (LPWSTR)pRemoteText;
    lvItem.cchTextMax = 260;

    SIZE_T written;
    WriteProcessMemory(hProcess, pRemoteItem, &lvItem, sizeof(LVITEMW), &written);
    SendMessageW(hListView, LVM_GETITEMTEXTW, index, (LPARAM)pRemoteItem);

    wchar_t buffer[260];
    SIZE_T bytesRead;
    ReadProcessMemory(hProcess, pRemoteText, buffer, 520, &bytesRead);
    buffer[259] = L'\0';

    return std::wstring(buffer);
}

void DesktopIcons::HideIcon(const std::wstring& fileName) {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    for (int i = 0; i < count; i++) {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        if (_wcsicmp(text.c_str(), fileName.c_str()) == 0) {
            int x = -5000;
            int y = -5000;
            LPARAM pos = MAKELPARAM(x & 0xFFFF, y);
            SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
            break;
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

bool DesktopIcons::IsPointOnIcon(int screenX, int screenY) {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return false;

    POINT pt = { screenX, screenY };
    ScreenToClient(hListView, &pt);

    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return false;

    LPVOID pRemoteInfo = VirtualAllocEx(hProcess, nullptr, 1024, MEM_COMMIT, PAGE_READWRITE);

    LVHITTESTINFO info = {};
    info.pt = pt;

    SIZE_T written;
    WriteProcessMemory(hProcess, pRemoteInfo, &info, sizeof(LVHITTESTINFO), &written);
    int index = (int)SendMessageW(hListView, LVM_HITTEST, 0, (LPARAM)pRemoteInfo);

    VirtualFreeEx(hProcess, pRemoteInfo, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return index != -1;
}

int DesktopIcons::GetSelectedCount() {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return 0;
    return (int)SendMessageW(hListView, LVM_GETSELECTEDCOUNT, 0, 0);
}

void DesktopIcons::RefreshDesktop() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void DesktopIcons::PositionIcon(const std::wstring& fileName, int x, int y) {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    for (int i = 0; i < count; i++) {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        if (_wcsicmp(text.c_str(), fileName.c_str()) == 0) {
            LPARAM pos = MAKELPARAM(x & 0xFFFF, y);
            SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
            break;
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

void DesktopIcons::PositionIcons(const std::map<std::wstring, POINT2D>& iconPositions) {
    if (iconPositions.empty()) return;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    for (int i = 0; i < count; i++) {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        // Case-insensitive lookup (display names may differ in case from filenames)
        for (const auto& [name, pos2d] : iconPositions) {
            if (_wcsicmp(text.c_str(), name.c_str()) == 0) {
                LPARAM pos = MAKELPARAM(pos2d.x & 0xFFFF, pos2d.y);
                SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
                break;
            }
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

POINT2D* DesktopIcons::GetIconPosition(const std::wstring& fileName) {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return nullptr;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return nullptr;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    POINT2D* result = nullptr;

    for (int i = 0; i < count; i++) {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        if (_wcsicmp(text.c_str(), fileName.c_str()) == 0) {
            LPVOID pRemotePoint = VirtualAllocEx(hProcess, nullptr, 8, MEM_COMMIT, PAGE_READWRITE);
            SendMessageW(hListView, LVM_GETITEMPOSITION, i, (LPARAM)pRemotePoint);

            POINT pt;
            SIZE_T bytesRead;
            ReadProcessMemory(hProcess, pRemotePoint, &pt, 8, &bytesRead);

            result = new POINT2D{ pt.x, pt.y };

            VirtualFreeEx(hProcess, pRemotePoint, 0, MEM_RELEASE);
            break;
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return result;
}

std::map<std::wstring, POINT2D> DesktopIcons::GetAllIconPositions() {
    std::map<std::wstring, POINT2D> result;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return result;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr) return result;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, 4096, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);
    LPVOID pRemotePoint = VirtualAllocEx(hProcess, nullptr, 8, MEM_COMMIT, PAGE_READWRITE);

    for (int i = 0; i < count; i++) {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);
        if (text.empty()) continue;

        SendMessageW(hListView, LVM_GETITEMPOSITION, i, (LPARAM)pRemotePoint);

        POINT pt;
        SIZE_T bytesRead;
        ReadProcessMemory(hProcess, pRemotePoint, &pt, 8, &bytesRead);

        result[text] = { pt.x, pt.y };
    }

    VirtualFreeEx(hProcess, pRemotePoint, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return result;
}

void DesktopIcons::SetIconsVisible(bool visible) {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return;

    ShowWindow(hListView, visible ? SW_SHOW : SW_HIDE);
}

bool DesktopIcons::AreIconsVisible() {
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr) return true;

    return IsWindowVisible(hListView) != FALSE;
}

// Registry key for shell icon overlays
static const wchar_t* SHELL_ICONS_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Icons";
static const wchar_t* SHORTCUT_ARROW_VALUE = L"29";

bool DesktopIcons::SetShortcutArrowsHidden(bool hidden) {
    HKEY hKey;
    LONG result;

    if (hidden) {
        // Create or open the Shell Icons key
        result = RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            SHELL_ICONS_KEY,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &hKey,
            nullptr
        );

        if (result != ERROR_SUCCESS) {
            // Try HKEY_CURRENT_USER as fallback (doesn't require admin)
            result = RegCreateKeyExW(
                HKEY_CURRENT_USER,
                SHELL_ICONS_KEY,
                0,
                nullptr,
                REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE,
                nullptr,
                &hKey,
                nullptr
            );
        }

        if (result != ERROR_SUCCESS) {
            return false;
        }

        // Set value "29" to an empty string to hide the arrow
        // Using a blank icon reference removes the overlay
        const wchar_t* blankValue = L"%systemroot%\\System32\\shell32.dll,-50";
        result = RegSetValueExW(
            hKey,
            SHORTCUT_ARROW_VALUE,
            0,
            REG_EXPAND_SZ,
            (const BYTE*)blankValue,
            (DWORD)((wcslen(blankValue) + 1) * sizeof(wchar_t))
        );

        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    else {
        // Delete the value to restore default arrow
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SHELL_ICONS_KEY, 0, KEY_SET_VALUE, &hKey);
        if (result == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, SHORTCUT_ARROW_VALUE);
            RegCloseKey(hKey);
        }

        result = RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_ICONS_KEY, 0, KEY_SET_VALUE, &hKey);
        if (result == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, SHORTCUT_ARROW_VALUE);
            RegCloseKey(hKey);
        }

        return true;
    }
}

bool DesktopIcons::AreShortcutArrowsHidden() {
    HKEY hKey;
    LONG result;

    // Check HKEY_LOCAL_MACHINE first
    result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SHELL_ICONS_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        DWORD type;
        DWORD size = 0;
        result = RegQueryValueExW(hKey, SHORTCUT_ARROW_VALUE, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS && size > 0) {
            return true;
        }
    }

    // Check HKEY_CURRENT_USER
    result = RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_ICONS_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        DWORD type;
        DWORD size = 0;
        result = RegQueryValueExW(hKey, SHORTCUT_ARROW_VALUE, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS && size > 0) {
            return true;
        }
    }

    return false;
}

void DesktopIcons::RestartExplorer() {
    // Find and terminate explorer.exe
    HWND shellWindow = GetShellWindow();
    if (shellWindow) {
        DWORD pid;
        GetWindowThreadProcessId(shellWindow, &pid);

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }

    // Wait briefly for explorer to terminate
    Sleep(1000);

    // Restart explorer
    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
}


std::vector<SpecialDesktopIcon> DesktopIcons::GetSpecialDesktopIcons() {
    std::vector<SpecialDesktopIcon> result;

    // Get the desktop IShellFolder
    IShellFolder* pDesktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&pDesktop))) return result;

    // Enumerate desktop items
    IEnumIDList* pEnum = nullptr;
    if (FAILED(pDesktop->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnum))) {
        pDesktop->Release();
        return result;
    }

    LPITEMIDLIST pidlChild = nullptr;
    while (pEnum->Next(1, &pidlChild, nullptr) == S_OK) {
        // Get the parsing name - special items return "::{GUID}"
        STRRET strret;
        if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidlChild, SHGDN_FORPARSING, &strret))) {
            wchar_t parseName[MAX_PATH] = {};
            StrRetToBufW(&strret, pidlChild, parseName, MAX_PATH);

            // Special shell items have parsing names starting with "::"
            if (parseName[0] == L':' && parseName[1] == L':') {
                // Get the localized display name
                STRRET strretDisplay;
                if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidlChild, SHGDN_NORMAL, &strretDisplay))) {
                    wchar_t displayName[MAX_PATH] = {};
                    StrRetToBufW(&strretDisplay, pidlChild, displayName, MAX_PATH);

                    SpecialDesktopIcon sdi;
                    // Strip leading "::" to store just the CLSID
                    sdi.clsid = parseName + 2;
                    sdi.displayName = displayName;
                    result.push_back(std::move(sdi));
                }
            }
        }
        CoTaskMemFree(pidlChild);
    }

    pEnum->Release();
    pDesktop->Release();
    return result;
}

std::wstring DesktopIcons::GetSpecialIconDisplayName(const std::wstring& clsid) {
    // Build the parsing name "::{CLSID}" and resolve to display name
    std::wstring parseName = L"::" + clsid;

    LPITEMIDLIST pidl = nullptr;
    if (FAILED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)) || !pidl) {
        return L"";
    }

    IShellFolder* pDesktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&pDesktop))) {
        CoTaskMemFree(pidl);
        return L"";
    }

    STRRET strret;
    std::wstring displayName;
    if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidl, SHGDN_NORMAL, &strret))) {
        wchar_t buf[MAX_PATH] = {};
        StrRetToBufW(&strret, pidl, buf, MAX_PATH);
        displayName = buf;
    }

    pDesktop->Release();
    CoTaskMemFree(pidl);
    return displayName;
}
