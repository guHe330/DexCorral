#pragma once
#include <Windows.h>
#include <string>

class TrayIcon {
public:
    static constexpr UINT WM_TRAYICON = WM_USER + 1;

    TrayIcon(HWND hwnd, HICON icon, const std::wstring& tooltip);
    ~TrayIcon();

    void Show();
    void Hide();
    void UpdateTooltip(const std::wstring& tooltip);

private:
    HWND hwnd;
    NOTIFYICONDATAW nid;
};
