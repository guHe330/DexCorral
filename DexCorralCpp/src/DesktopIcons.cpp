#include "DesktopIcons.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <vector>
#include <algorithm>

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

        auto it = iconPositions.find(text);
        if (it != iconPositions.end()) {
            LPARAM pos = MAKELPARAM(it->second.x & 0xFFFF, it->second.y);
            SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
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
