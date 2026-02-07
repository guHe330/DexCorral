#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>

struct POINT2D {
    int x;
    int y;
};

// Represents a special shell icon on the desktop (Recycle Bin, This PC, etc.)
struct SpecialDesktopIcon {
    std::wstring clsid;        // CLSID string e.g. "{645FF040-5081-101B-9F08-00AA002F954E}"
    std::wstring displayName;  // Localized display name e.g. "Recycle Bin"
};

class DesktopIcons {
public:
    static void HideIcon(const std::wstring& fileName);
    static bool IsPointOnIcon(int screenX, int screenY);
    static int GetSelectedCount();
    static void RefreshDesktop();
    static void PositionIcon(const std::wstring& fileName, int x, int y);
    static void PositionIcons(const std::map<std::wstring, POINT2D>& iconPositions);
    static POINT2D* GetIconPosition(const std::wstring& fileName);

    // Hide/show all desktop icons (the entire ListView)
    static void SetIconsVisible(bool visible);
    static bool AreIconsVisible();

    // Shortcut arrow overlay control (requires Explorer restart to take effect)
    static bool SetShortcutArrowsHidden(bool hidden);
    static bool AreShortcutArrowsHidden();
    static void RestartExplorer();

    // Experiment: Hide icons by deleting from ListView (requires refresh to restore)
    static bool HideRandomIconExperiment();  // Deletes a random icon from ListView, returns success
    static void RestoreHiddenIconsExperiment();  // Refreshes desktop to restore deleted items
    static int GetIconCount();  // Returns number of desktop icons

    static HWND GetDesktopListView();

    // Special icon support - enumerate virtual shell items on desktop by CLSID
    static std::vector<SpecialDesktopIcon> GetSpecialDesktopIcons();
    static std::wstring GetSpecialIconDisplayName(const std::wstring& clsid);

private:
    static std::wstring GetItemText(HWND hListView, HANDLE hProcess, LPVOID pRemoteItem, LPVOID pRemoteText, int index);
};
