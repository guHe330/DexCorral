/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "MonitorManager.h"
#include "LayoutMath.h"
#include <algorithm>

// Need to link with user32.lib for QueryDisplayConfig
#pragma comment(lib, "user32.lib")

MonitorManager::MonitorManager()
{
    Refresh();
}

void MonitorManager::Refresh()
{
    monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)this);
}

BOOL CALLBACK MonitorManager::MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT rect, LPARAM lParam)
{
    MonitorManager *self = (MonitorManager *)lParam;

    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi))
    {
        return TRUE;
    }

    MonitorInfo info;
    info.deviceName = mi.szDevice;
    info.bounds = mi.rcMonitor;
    info.workArea = mi.rcWork;
    info.width = mi.rcMonitor.right - mi.rcMonitor.left;
    info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.deviceId = GetMonitorDeviceId(hMon);

    self->monitors.push_back(info);
    return TRUE;
}

std::string MonitorManager::GetMonitorDeviceId(HMONITOR hMon)
{
    // Get the number of paths and modes
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
    {
        return "";
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(),
                           &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
    {
        return "";
    }

    // Get the monitor info to match against
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi))
    {
        return "";
    }

    // Find the path that matches this monitor's device name
    for (const auto &path : paths)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = path.sourceInfo.adapterId;
        sourceName.header.id = path.sourceInfo.id;

        if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
        {
            if (wcscmp(sourceName.viewGdiDeviceName, mi.szDevice) == 0)
            {
                // Found matching path, get target device name (hardware ID)
                DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
                targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                targetName.header.size = sizeof(targetName);
                targetName.header.adapterId = path.targetInfo.adapterId;
                targetName.header.id = path.targetInfo.id;

                if (DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS)
                {
                    // The monitor's device interface path is the stable ID, e.g.
                    // "\\?\DISPLAY#GSM5B7F#5&1a2b3c4d&0&UID4353#{e6f07b5f-...}"
                    std::wstring wpath = targetName.monitorDevicePath;

                    // Convert to narrow string for storage
                    int size = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (size > 0)
                    {
                        std::string result(size - 1, 0);
                        WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], size, nullptr, nullptr);
                        return result;
                    }
                }
            }
        }
    }

    // Fallback: use GDI device name (less stable but works)
    int size = WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0)
    {
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, &result[0], size, nullptr, nullptr);
        return result;
    }

    return "";
}

std::string MonitorManager::GetTopologyKey() const
{
    std::vector<std::string> keys;
    for (const auto &mon : monitors)
        keys.push_back(LayoutMath::MonitorKey(mon.deviceId, mon.width, mon.height));
    return LayoutMath::TopologyKey(keys);
}

const MonitorInfo *MonitorManager::FindMonitor(const std::string &deviceId) const
{
    for (const auto &mon : monitors)
    {
        if (mon.deviceId == deviceId)
        {
            return &mon;
        }
    }
    return nullptr;
}

const MonitorInfo *MonitorManager::FindMonitorAt(int x, int y) const
{
    POINT pt = {x, y};
    for (const auto &mon : monitors)
    {
        if (PtInRect(&mon.bounds, pt))
        {
            return &mon;
        }
    }
    return nullptr;
}

const MonitorInfo *MonitorManager::FindMonitorForRect(const RECT &rect) const
{
    // The monitor with the largest share of the rect, like MonitorFromRect.
    // Not the centre point: that can fall into a gap between monitors.
    const MonitorInfo *best = nullptr;
    LONGLONG bestArea = 0;
    for (const auto &mon : monitors)
    {
        RECT overlap;
        if (!IntersectRect(&overlap, &mon.bounds, &rect))
            continue;
        LONGLONG area = (LONGLONG)(overlap.right - overlap.left) * (overlap.bottom - overlap.top);
        if (area > bestArea)
        {
            bestArea = area;
            best = &mon;
        }
    }
    return best;
}

const MonitorInfo *MonitorManager::GetPrimaryMonitor() const
{
    for (const auto &mon : monitors)
    {
        if (mon.isPrimary)
        {
            return &mon;
        }
    }
    // Fallback to first monitor
    return monitors.empty() ? nullptr : &monitors[0];
}
