#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>

struct MonitorInfo {
    std::string deviceId;       // Hardware ID (stable across reboots)
    std::wstring deviceName;    // GDI device name (e.g., \\.\DISPLAY1)
    RECT workArea;              // Usable area (excluding taskbar)
    RECT bounds;                // Full monitor bounds
    int width;                  // Current width
    int height;                 // Current height
    bool isPrimary;
};

class MonitorManager {
public:
    MonitorManager();

    // Refresh monitor list (call on WM_DISPLAYCHANGE)
    void Refresh();

    // Get all active monitors
    const std::vector<MonitorInfo>& GetMonitors() const { return monitors; }

    // Find monitor by hardware ID
    const MonitorInfo* FindMonitor(const std::string& deviceId) const;

    // Find monitor containing a point
    const MonitorInfo* FindMonitorAt(int x, int y) const;

    // Find monitor containing a rectangle (by center point)
    const MonitorInfo* FindMonitorForRect(const RECT& rect) const;

    // Get primary monitor
    const MonitorInfo* GetPrimaryMonitor() const;

    // Check if a monitor ID is currently active
    bool IsMonitorActive(const std::string& deviceId) const;

    // Get hardware ID from HMONITOR
    static std::string GetMonitorDeviceId(HMONITOR hMon);

private:
    std::vector<MonitorInfo> monitors;

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT rect, LPARAM lParam);
};
