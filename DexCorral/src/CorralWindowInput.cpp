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

/**
 * CorralWindowInput.cpp - Mouse/keyboard handling, resize, drag-drop, and file operations
 *
 * Implements mouse event handling (clicks, drag-drop, hover effects), keyboard navigation,
 * window resizing and snapping, file operations (copy, cut, paste, delete), and scrollbar
 * management. Integrates with the Explorer hook for proper interaction with hidden desktop icons.
 */

#include "CorralWindow.h"
#include "App.h"
#include "DesktopIcons.h"
#include "IconUtils.h"
#include <windowsx.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <algorithm>
#include <Shlwapi.h>
#include <cmath>
#include <stdio.h>

// ============================================================================
// Drag-drop logging (writes to %APPDATA%/DexCorral/CorralDrop.log)
// ============================================================================

static wchar_t g_CorralDropLogPath[MAX_PATH] = {};

static void LogCorralDrop(const wchar_t *fmt, ...)
{
    if (!g_CorralDropLogPath[0])
    {
        wchar_t *appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        {
            wchar_t dir[MAX_PATH];
            swprintf_s(dir, L"%s\\DexCorral", appData);
            CreateDirectoryW(dir, nullptr);
            swprintf_s(g_CorralDropLogPath, L"%s\\CorralDrop.log", dir);
            CoTaskMemFree(appData);
        }
        else
        {
            GetTempPathW(MAX_PATH, g_CorralDropLogPath);
            wcscat_s(g_CorralDropLogPath, L"CorralDrop.log");
        }
    }

    HANDLE hFile = CreateFileW(g_CorralDropLogPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buffer[1024];
        int offset = swprintf_s(buffer, L"[%02d:%02d:%02d.%03d] ",
                                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        va_list args;
        va_start(args, fmt);
        vswprintf_s(buffer + offset, _countof(buffer) - offset, fmt, args);
        va_end(args);

        wcscat_s(buffer, L"\r\n");
        DWORD written;
        WriteFile(hFile, buffer, (DWORD)(wcslen(buffer) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }
}

// ============================================================================
// Scrollbar support
// ============================================================================

bool CorralWindow::NeedsScrollbar() const
{
    return contentHeight > GetVisibleHeight();
}

int CorralWindow::GetContentHeight() const
{
    return contentHeight;
}

int CorralWindow::GetVisibleHeight() const
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    return rect.bottom;
}

RECT CorralWindow::GetScrollbarTrackRect() const
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    return {
        rect.right - Dpi(SCROLLBAR_WIDTH) - Dpi(SCROLLBAR_MARGIN),
        GetIconAreaTop() + Dpi(SCROLLBAR_MARGIN),
        rect.right - Dpi(SCROLLBAR_MARGIN),
        rect.bottom - Dpi(SCROLLBAR_MARGIN)};
}

RECT CorralWindow::GetScrollbarThumbRect() const
{
    if (!NeedsScrollbar())
        return {0, 0, 0, 0};

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();

    // When hovered, adjust for arrow buttons
    int arrowSize = isScrollbarHovered ? Dpi(SCROLLBAR_ARROW_SIZE) : 0;
    int effectiveTrackTop = track.top + arrowSize;
    int effectiveTrackHeight = trackHeight - 2 * arrowSize;

    // Thumb size proportional to visible/content ratio
    int thumbHeight = (std::max)(Dpi(SCROLLBAR_MIN_THUMB),
                                 (int)((float)visibleHeight / contentHeight * effectiveTrackHeight));

    // Thumb position based on scroll position
    int scrollRange = contentHeight - visibleHeight;
    int thumbRange = effectiveTrackHeight - thumbHeight;
    int thumbTop = effectiveTrackTop;
    if (scrollRange > 0)
    {
        thumbTop = effectiveTrackTop + (int)((float)scrollPosition / scrollRange * thumbRange);
    }

    return {
        track.left,
        thumbTop,
        track.right,
        thumbTop + thumbHeight};
}

bool CorralWindow::HitTestScrollbar(int x, int y) const
{
    if (!NeedsScrollbar())
        return false;
    RECT track = GetScrollbarTrackRect();
    POINT pt = {x, y};
    return PtInRect(&track, pt) != FALSE;
}

bool CorralWindow::HitTestScrollbarThumb(int x, int y) const
{
    if (!NeedsScrollbar())
        return false;
    RECT thumb = GetScrollbarThumbRect();
    POINT pt = {x, y};
    return PtInRect(&thumb, pt) != FALSE;
}

void CorralWindow::OnMouseWheel(int delta)
{
    // Only scroll if content exceeds visible area
    if (!NeedsScrollbar())
        return;

    // Scroll proportionally - multiply first to avoid integer truncation for small deltas
    // (precision touchpads send small incremental deltas like 6, 20, etc. instead of 120)
    int scrollAmount = delta * iconSpacingY / WHEEL_DELTA;
    scrollPosition -= scrollAmount;
    ClampScrollPosition();
    UpdateLayeredContent();

    // Debounced reposition of real desktop icons to match new scroll state
    KillTimer(hwnd, SCROLL_REPOSITION_TIMER_ID);
    SetTimer(hwnd, SCROLL_REPOSITION_TIMER_ID, SCROLL_REPOSITION_DEBOUNCE_MS, nullptr);
}

void CorralWindow::StartScrollbarDrag(int y)
{
    isDraggingScrollbar = true;
    scrollbarDragStartY = y;
    scrollbarDragStartPos = scrollPosition;
    SetCapture(hwnd);
}

void CorralWindow::DoScrollbarDrag(int y)
{
    if (!isDraggingScrollbar)
        return;

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();

    // Adjust for arrow buttons when hovered
    int arrowSize = isScrollbarHovered ? Dpi(SCROLLBAR_ARROW_SIZE) : 0;
    int effectiveTrackHeight = trackHeight - 2 * arrowSize;

    int thumbHeight = (std::max)(Dpi(SCROLLBAR_MIN_THUMB),
                                 (int)((float)visibleHeight / contentHeight * effectiveTrackHeight));
    int thumbRange = effectiveTrackHeight - thumbHeight;
    int scrollRange = contentHeight - visibleHeight;

    if (thumbRange > 0 && scrollRange > 0)
    {
        int deltaY = y - scrollbarDragStartY;
        int newScrollPos = scrollbarDragStartPos + (int)((float)deltaY / thumbRange * scrollRange);
        scrollPosition = newScrollPos;
        ClampScrollPosition();
        UpdateLayeredContent();
    }
}

void CorralWindow::EndScrollbarDrag()
{
    if (isDraggingScrollbar)
    {
        isDraggingScrollbar = false;
        ReleaseCapture();

        // Reposition real desktop icons to match final scroll state
        KillTimer(hwnd, SCROLL_REPOSITION_TIMER_ID);
        SetTimer(hwnd, SCROLL_REPOSITION_TIMER_ID, SCROLL_REPOSITION_DEBOUNCE_MS, nullptr);
    }
}

void CorralWindow::ClampScrollPosition()
{
    int maxScroll = contentHeight - GetVisibleHeight();
    if (maxScroll < 0)
        maxScroll = 0;
    if (scrollPosition < 0)
        scrollPosition = 0;
    if (scrollPosition > maxScroll)
        scrollPosition = maxScroll;
}

// ============================================================================
// Resize support
// ============================================================================

int CorralWindow::HitTestResize(int x, int y)
{
    RECT rect;
    GetClientRect(hwnd, &rect);

    bool nearLeft = (x <= Dpi(RESIZE_BORDER));
    bool nearRight = (x >= rect.right - Dpi(RESIZE_BORDER));
    bool nearTop = (y <= Dpi(RESIZE_BORDER) && y > 0); // Don't conflict with title bar at y=0
    bool nearBottom = (y >= rect.bottom - Dpi(RESIZE_BORDER));

    // When rolled up (minimized), disable resizing completely
    if (config.IsRolledUp)
    {
        return 0;
    }

    // Corners first (they take priority)
    if (nearLeft && nearTop)
        return HTTOPLEFT;
    if (nearRight && nearTop)
        return HTTOPRIGHT;
    if (nearLeft && nearBottom)
        return HTBOTTOMLEFT;
    if (nearRight && nearBottom)
        return HTBOTTOMRIGHT;

    // Then edges
    if (nearLeft)
        return HTLEFT;
    if (nearRight)
        return HTRIGHT;
    if (nearTop)
        return HTTOP;
    if (nearBottom)
        return HTBOTTOM;

    return 0;
}

void CorralWindow::StartResize(int hitTest, int x, int y)
{
    isResizing = true;
    resizeMode = hitTest;
    GetCursorPos(&resizeStart);
    GetWindowRect(hwnd, &resizeStartRect);
    SetCapture(hwnd);
}

void CorralWindow::DoResize(int screenX, int screenY)
{
    if (!isResizing)
        return;

    int dx = screenX - resizeStart.x;
    int dy = screenY - resizeStart.y;

    int newLeft = resizeStartRect.left;
    int newTop = resizeStartRect.top;
    int newWidth = resizeStartRect.right - resizeStartRect.left;
    int newHeight = resizeStartRect.bottom - resizeStartRect.top;

    // Handle horizontal resizing
    if (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT)
    {
        newLeft += dx;
        newWidth -= dx;
    }
    if (resizeMode == HTRIGHT || resizeMode == HTTOPRIGHT || resizeMode == HTBOTTOMRIGHT)
    {
        newWidth += dx;
    }

    // Handle vertical resizing
    if (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT)
    {
        newTop += dy;
        newHeight -= dy;
    }
    if (resizeMode == HTBOTTOM || resizeMode == HTBOTTOMLEFT || resizeMode == HTBOTTOMRIGHT)
    {
        newHeight += dy;
    }

    // Minimum size constraints
    if (newWidth < 100)
    {
        if (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT)
        {
            newLeft = resizeStartRect.right - 100;
        }
        newWidth = 100;
    }
    if (newHeight < 80)
    {
        if (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT)
        {
            newTop = resizeStartRect.bottom - 80;
        }
        newHeight = 80;
    }

    // Apply snap to resize unless Shift is held
    if (!(GetKeyState(VK_SHIFT) & 0x8000))
    {
        ApplyResizeSnap(newLeft, newTop, newWidth, newHeight, resizeMode);
    }

    SetWindowPos(hwnd, nullptr, newLeft, newTop, newWidth, newHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void CorralWindow::EndResize()
{
    if (isResizing)
    {
        isResizing = false;
        ReleaseCapture();
        SyncConfigFromWindow();
        CalculateIconLayout();
        UpdateLayeredContent();
        if (App::GetInstance())
        {
            // Push desktop icons that may now be under the resized corral
            App::GetInstance()->CacheDesktopIconPositions();
            App::GetInstance()->PushDesktopIconsFromCorrals();
            App::GetInstance()->InvalidateDesktopIconCache();
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Snap support
// ============================================================================

void CorralWindow::ApplySnap(int &newLeft, int &newTop, int width, int height)
{
    App *app = App::GetInstance();
    if (!app)
        return;

    int newRight = newLeft + width;
    int newBottom = newTop + height;

    // Snap to work area edges of the monitor the corral is on
    RECT corralRect = {newLeft, newTop, newRight, newBottom};
    HMONITOR hMon = MonitorFromRect(&corralRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMon, &mi);
    RECT wa = mi.rcWork; // Work area (excludes taskbar)

    // Left edge
    if (std::abs(newLeft - wa.left) < Dpi(SNAP_DISTANCE))
    {
        newLeft = wa.left + Dpi(SNAP_GAP);
    }
    // Right edge
    if (std::abs(newRight - wa.right) < Dpi(SNAP_DISTANCE))
    {
        newLeft = wa.right - width - Dpi(SNAP_GAP);
    }
    // Top edge
    if (std::abs(newTop - wa.top) < Dpi(SNAP_DISTANCE))
    {
        newTop = wa.top + Dpi(SNAP_GAP);
    }
    // Bottom edge
    if (std::abs(newBottom - wa.bottom) < Dpi(SNAP_DISTANCE))
    {
        newTop = wa.bottom - height - Dpi(SNAP_GAP);
    }

    // Snap to other corrals
    const auto &corrals = app->GetCorrals();
    for (const auto &other : corrals)
    {
        if (other->GetHWND() == hwnd)
            continue; // Skip self

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        int otherLeft = otherRect.left;
        int otherTop = otherRect.top;
        int otherRight = otherRect.right;
        int otherBottom = otherRect.bottom;

        // Adjacent snapping (with gap) - only when corrals are nearby
        bool verticalOverlap = (newTop < otherBottom + Dpi(SNAP_DISTANCE)) && (newBottom > otherTop - Dpi(SNAP_DISTANCE));
        bool horizontalOverlap = (newLeft < otherRight + Dpi(SNAP_DISTANCE)) && (newRight > otherLeft - Dpi(SNAP_DISTANCE));

        if (verticalOverlap)
        {
            // Snap our right edge to their left edge (with gap)
            if (std::abs(newRight - otherLeft + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
            {
                newLeft = otherLeft - width - Dpi(SNAP_GAP);
            }
            // Snap our left edge to their right edge (with gap)
            if (std::abs(newLeft - otherRight - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
            {
                newLeft = otherRight + Dpi(SNAP_GAP);
            }
        }

        if (horizontalOverlap)
        {
            // Snap our bottom edge to their top edge (with gap)
            if (std::abs(newBottom - otherTop + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
            {
                newTop = otherTop - height - Dpi(SNAP_GAP);
            }
            // Snap our top edge to their bottom edge (with gap)
            if (std::abs(newTop - otherBottom - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
            {
                newTop = otherBottom + Dpi(SNAP_GAP);
            }
        }

        // Alignment snapping - works across the whole screen
        // Align top edges
        if (std::abs(newTop - otherTop) < Dpi(SNAP_DISTANCE))
        {
            newTop = otherTop;
        }
        // Align bottom edges
        if (std::abs(newTop + height - otherBottom) < Dpi(SNAP_DISTANCE))
        {
            newTop = otherBottom - height;
        }
        // Align our top to their bottom (with gap)
        if (std::abs(newTop - otherBottom - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
        {
            newTop = otherBottom + Dpi(SNAP_GAP);
        }
        // Align our bottom to their top (with gap)
        if (std::abs(newTop + height - otherTop + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
        {
            newTop = otherTop - height - Dpi(SNAP_GAP);
        }
        // Align left edges
        if (std::abs(newLeft - otherLeft) < Dpi(SNAP_DISTANCE))
        {
            newLeft = otherLeft;
        }
        // Align right edges
        if (std::abs(newLeft + width - otherRight) < Dpi(SNAP_DISTANCE))
        {
            newLeft = otherRight - width;
        }
        // Align our left to their right (with gap)
        if (std::abs(newLeft - otherRight - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
        {
            newLeft = otherRight + Dpi(SNAP_GAP);
        }
        // Align our right to their left (with gap)
        if (std::abs(newLeft + width - otherLeft + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
        {
            newLeft = otherLeft - width - Dpi(SNAP_GAP);
        }
    }
}

void CorralWindow::ApplyResizeSnap(int &newLeft, int &newTop, int &newWidth, int &newHeight, int resizeMode)
{
    App *app = App::GetInstance();
    if (!app)
        return;

    // Determine which edges are being resized
    bool resizingLeft = (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT);
    bool resizingRight = (resizeMode == HTRIGHT || resizeMode == HTTOPRIGHT || resizeMode == HTBOTTOMRIGHT);
    bool resizingTop = (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT);
    bool resizingBottom = (resizeMode == HTBOTTOM || resizeMode == HTBOTTOMLEFT || resizeMode == HTBOTTOMRIGHT);

    int newRight = newLeft + newWidth;
    int newBottom = newTop + newHeight;

    // Snap to work area edges of the monitor the corral is on
    RECT corralRect = {newLeft, newTop, newRight, newBottom};
    HMONITOR hMon = MonitorFromRect(&corralRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMon, &mi);
    RECT wa = mi.rcWork;

    // Left edge
    if (resizingLeft && std::abs(newLeft - wa.left - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
    {
        int oldRight = newRight;
        newLeft = wa.left + Dpi(SNAP_GAP);
        newWidth = oldRight - newLeft;
    }
    // Right edge
    if (resizingRight && std::abs(newRight - wa.right + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
    {
        newWidth = wa.right - newLeft - Dpi(SNAP_GAP);
    }
    // Top edge
    if (resizingTop && std::abs(newTop - wa.top - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
    {
        int oldBottom = newBottom;
        newTop = wa.top + Dpi(SNAP_GAP);
        newHeight = oldBottom - newTop;
    }
    // Bottom edge
    if (resizingBottom && std::abs(newBottom - wa.bottom + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
    {
        newHeight = wa.bottom - newTop - Dpi(SNAP_GAP);
    }

    // Recalculate edges after screen snap
    newRight = newLeft + newWidth;
    newBottom = newTop + newHeight;

    // Snap to other corrals on the same monitor
    const auto &corrals = app->GetCorrals();
    for (const auto &other : corrals)
    {
        if (other->GetHWND() == hwnd)
            continue;

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        // Only snap to corrals on the same monitor
        if (MonitorFromRect(&otherRect, MONITOR_DEFAULTTONEAREST) != hMon)
            continue;

        // Check overlap for adjacent snapping (with gap)
        bool verticalOverlap = (newTop < otherRect.bottom + Dpi(SNAP_DISTANCE)) && (newBottom > otherRect.top - Dpi(SNAP_DISTANCE));
        bool horizontalOverlap = (newLeft < otherRect.right + Dpi(SNAP_DISTANCE)) && (newRight > otherRect.left - Dpi(SNAP_DISTANCE));

        // Adjacent snapping (with gap) - requires overlap in the perpendicular dimension
        if (verticalOverlap)
        {
            if (resizingLeft)
            {
                if (std::abs(newLeft - otherRect.right - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
                {
                    int oldRight = newRight;
                    newLeft = otherRect.right + Dpi(SNAP_GAP);
                    newWidth = oldRight - newLeft;
                }
            }
            if (resizingRight)
            {
                if (std::abs(newRight - otherRect.left + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
                {
                    newWidth = otherRect.left - newLeft - Dpi(SNAP_GAP);
                }
            }
        }
        if (horizontalOverlap)
        {
            if (resizingTop)
            {
                if (std::abs(newTop - otherRect.bottom - Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
                {
                    int oldBottom = newBottom;
                    newTop = otherRect.bottom + Dpi(SNAP_GAP);
                    newHeight = oldBottom - newTop;
                }
            }
            if (resizingBottom)
            {
                if (std::abs(newBottom - otherRect.top + Dpi(SNAP_GAP)) < Dpi(SNAP_DISTANCE))
                {
                    newHeight = otherRect.top - newTop - Dpi(SNAP_GAP);
                }
            }
        }

        // Alignment snapping - align edges with any corral on the same monitor
        if (resizingLeft)
        {
            if (std::abs(newLeft - otherRect.left) < Dpi(SNAP_DISTANCE))
            {
                int oldRight = newRight;
                newLeft = otherRect.left;
                newWidth = oldRight - newLeft;
            }
            if (std::abs(newLeft - otherRect.right) < Dpi(SNAP_DISTANCE))
            {
                int oldRight = newRight;
                newLeft = otherRect.right;
                newWidth = oldRight - newLeft;
            }
        }
        if (resizingRight)
        {
            if (std::abs(newRight - otherRect.right) < Dpi(SNAP_DISTANCE))
            {
                newWidth = otherRect.right - newLeft;
            }
            if (std::abs(newRight - otherRect.left) < Dpi(SNAP_DISTANCE))
            {
                newWidth = otherRect.left - newLeft;
            }
        }
        if (resizingTop)
        {
            if (std::abs(newTop - otherRect.top) < Dpi(SNAP_DISTANCE))
            {
                int oldBottom = newBottom;
                newTop = otherRect.top;
                newHeight = oldBottom - newTop;
            }
            if (std::abs(newTop - otherRect.bottom) < Dpi(SNAP_DISTANCE))
            {
                int oldBottom = newBottom;
                newTop = otherRect.bottom;
                newHeight = oldBottom - newTop;
            }
        }
        if (resizingBottom)
        {
            if (std::abs(newBottom - otherRect.bottom) < Dpi(SNAP_DISTANCE))
            {
                newHeight = otherRect.bottom - newTop;
            }
            if (std::abs(newBottom - otherRect.top) < Dpi(SNAP_DISTANCE))
            {
                newHeight = otherRect.top - newTop;
            }
        }

        // Recalculate after each corral snap for next iteration
        newRight = newLeft + newWidth;
        newBottom = newTop + newHeight;
    }

    // Enforce minimum size (adjusting position for left/top edge resizing)
    if (newWidth < 100)
    {
        if (resizingLeft)
        {
            newLeft -= (100 - newWidth);
        }
        newWidth = 100;
    }
    if (newHeight < 80)
    {
        if (resizingTop)
        {
            newTop -= (80 - newHeight);
        }
        newHeight = 80;
    }
}

// ============================================================================
// Mouse event handlers
// ============================================================================

void CorralWindow::OnLeftButtonDown(int x, int y)
{
    // Check resize area first (but not when rolled up)
    if (!config.IsRolledUp)
    {
        int resizeHit = HitTestResize(x, y);
        if (resizeHit)
        {
            StartResize(resizeHit, x, y);
            return;
        }
    }

    // Check if clicked on scrollbar
    if (HitTestScrollbar(x, y))
    {
        // Check if clicked on arrow buttons (when hovered)
        if (isScrollbarHovered)
        {
            RECT track = GetScrollbarTrackRect();
            int arrowSize = Dpi(SCROLLBAR_ARROW_SIZE);

            // Top arrow button
            if (y >= track.top && y < track.top + arrowSize)
            {
                // Scroll up
                scrollPosition -= iconSpacingY;
                ClampScrollPosition();
                UpdateLayeredContent();
                return;
            }

            // Bottom arrow button
            if (y >= track.bottom - arrowSize && y < track.bottom)
            {
                // Scroll down
                scrollPosition += iconSpacingY;
                ClampScrollPosition();
                UpdateLayeredContent();
                return;
            }
        }

        if (HitTestScrollbarThumb(x, y))
        {
            // Clicked on thumb - start dragging
            StartScrollbarDrag(y);
        }
        else
        {
            // Clicked in track - jump to position
            RECT track = GetScrollbarTrackRect();
            RECT thumb = GetScrollbarThumbRect();
            int thumbHeight = thumb.bottom - thumb.top;
            int trackHeight = track.bottom - track.top;

            // Adjust for arrows when hovered
            int arrowSize = isScrollbarHovered ? Dpi(SCROLLBAR_ARROW_SIZE) : 0;
            int effectiveTrackHeight = trackHeight - 2 * arrowSize;
            int thumbRange = effectiveTrackHeight - thumbHeight;
            int scrollRange = contentHeight - GetVisibleHeight();

            // Calculate new scroll position - center thumb on click point
            int clickOffset = y - (track.top + arrowSize) - (thumbHeight / 2);
            if (clickOffset < 0)
                clickOffset = 0;
            if (clickOffset > thumbRange)
                clickOffset = thumbRange;

            if (thumbRange > 0)
            {
                scrollPosition = (int)((float)clickOffset / thumbRange * scrollRange);
                ClampScrollPosition();
                UpdateLayeredContent();
            }
        }
        return;
    }

    // Check if clicked on a tab in title bar
    int tabHit = HitTestTab(x, y);
    if (tabHit >= 0)
    {
        if (tabHit != config.ActiveTabIndex)
        {
            SetActiveTab(tabHit);
        }
        // Allow dragging window from tab
        isDragging = true;
        GetCursorPos(&dragStart);
        GetWindowRect(hwnd, &dragStartRect);

        if (App::GetInstance())
        {
            App::GetInstance()->CacheDesktopIconPositions();
        }
        SetCapture(hwnd);
        return;
    }

    // Check if clicked on an icon - select it (drag starts on mouse move)
    int hit = HitTestIcon(x, y);
    if (hit >= 0)
    {
        // Check if we clicked on the label of an already-selected icon
        if (hit == selectedIcon && HitTestIconLabel(x, y, hit))
        {
            // Clicked on label of selected icon - start rename
            StartIconRename(hit);
            return;
        }

        // Otherwise, just select the icon
        selectedIcon = hit;
        draggedIconIndex = hit; // Potential drag candidate
        iconDragStart = {x, y}; // Store starting position
        dropTargetIndex = -1;
        SetFocus(hwnd); // Take keyboard focus so F2 works
        SetCapture(hwnd);
        UpdateLayeredContent(); // Show selection highlight
        return;
    }

    // Deselect
    if (selectedIcon >= 0)
    {
        selectedIcon = -1;
        UpdateLayeredContent();
    }

    // Title bar - start dragging window (empty area)
    if (y < GetTitleBarHeight())
    {
        isDragging = true;
        GetCursorPos(&dragStart);
        GetWindowRect(hwnd, &dragStartRect);

        if (App::GetInstance())
        {
            App::GetInstance()->CacheDesktopIconPositions();
        }
        SetCapture(hwnd);
    }
}

void CorralWindow::OnLeftButtonDblClick(int x, int y)
{
    // Double-click on title bar toggles roll-up
    if (y < GetTitleBarHeight())
    {
        ToggleRollUp();
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0)
    {
        OpenFile(hit);
    }
}

void CorralWindow::OnLeftButtonUp(int x, int y)
{
    if (isResizing)
    {
        EndResize();
    }
    else if (isDraggingScrollbar)
    {
        EndScrollbarDrag();
    }
    else if (isDraggingIcon)
    {
        OnIconDragEnd();
        ReleaseCapture();
    }
    else if (draggedIconIndex >= 0)
    {
        // Mouse up without dragging - just a selection click
        draggedIconIndex = -1;
        ReleaseCapture();
    }
    else if (isDragging)
    {
        isDragging = false;
        ReleaseCapture();

        // Check for merge with another corral
        POINT pt;
        GetCursorPos(&pt);
        App *app = App::GetInstance();
        if (app)
        {
            for (const auto &other : app->GetCorrals())
            {
                if (other.get() == this)
                    continue;

                RECT otherRect;
                GetWindowRect(other->GetHWND(), &otherRect);

                // If dropped on another window's title bar, merge
                if (PtInRect(&otherRect, pt) && (pt.y - otherRect.top < other->GetTitleBarHeight()))
                {
                    MergeWith(other.get());
                    return; // This window is destroyed now
                }
            }
        }

        SyncConfigFromWindow();
        if (App::GetInstance())
        {
            App::GetInstance()->InvalidateDesktopIconCache();
            App::GetInstance()->SaveConfig();
        }
    }
}

void CorralWindow::OnRightButtonDown(int x, int y)
{
    // Check title bar first - don't let scrolled icons consume clicks here
    if (y < GetTitleBarHeight())
    {
        ShowContextMenu(x, y);
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0)
    {
        selectedIcon = hit;
        UpdateLayeredContent();

        POINT pt = {x, y};
        ClientToScreen(hwnd, &pt);
        ShowShellContextMenu(hit, pt.x, pt.y);
        return;
    }

    ShowContextMenu(x, y);
}

// ============================================================================
// File operations
// ============================================================================

void CorralWindow::OpenFile(int iconIndex)
{
    if (iconIndex < 0 || iconIndex >= (int)icons.size())
        return;

    const auto &icon = icons[iconIndex];

    if (icon.isSpecialIcon)
    {
        // Open special shell item via PIDL
        std::wstring parseName = L"::" + icon.clsid;
        LPITEMIDLIST pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)) && pidl)
        {
            SHELLEXECUTEINFOW sei = {sizeof(sei)};
            sei.lpIDList = pidl;
            sei.fMask = SEE_MASK_IDLIST;
            sei.nShow = SW_SHOWNORMAL;
            sei.hwnd = hwnd;
            ShellExecuteExW(&sei);
            CoTaskMemFree(pidl);
        }
        return;
    }

    ShellExecuteW(hwnd, L"open", icon.fullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CorralWindow::ShowShellContextMenu(int iconIndex, int screenX, int screenY)
{
    if (iconIndex < 0 || iconIndex >= (int)icons.size())
        return;

    const auto &icon = icons[iconIndex];

    LPITEMIDLIST pidlFull = nullptr;
    HRESULT hr;
    if (icon.isSpecialIcon)
    {
        std::wstring parseName = L"::" + icon.clsid;
        hr = SHParseDisplayName(parseName.c_str(), nullptr, &pidlFull, 0, nullptr);
    }
    else
    {
        hr = SHParseDisplayName(icon.fullPath.c_str(), nullptr, &pidlFull, 0, nullptr);
    }
    if (FAILED(hr) || !pidlFull)
        return;

    IShellFolder *pParentFolder = nullptr;
    LPCITEMIDLIST pidlChild = nullptr;
    hr = SHBindToParent(pidlFull, IID_IShellFolder, (void **)&pParentFolder, &pidlChild);
    if (FAILED(hr) || !pParentFolder)
    {
        CoTaskMemFree(pidlFull);
        return;
    }

    IContextMenu *pContextMenu = nullptr;
    hr = pParentFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, nullptr, (void **)&pContextMenu);
    if (FAILED(hr) || !pContextMenu)
    {
        pParentFolder->Release();
        CoTaskMemFree(pidlFull);
        return;
    }

    HMENU hMenu = CreatePopupMenu();
    if (hMenu)
    {
        hr = pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL | CMF_EXPLORE);
        if (SUCCEEDED(hr))
        {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 0x7FFF + 1, L"Remove from Corral");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     screenX, screenY, 0, hwnd, nullptr);
            PostMessageW(hwnd, WM_NULL, 0, 0);
            SendToBottom();

            if (cmd == 0x7FFF + 1)
            {
                auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), icon.fileName);
                if (it != GetActiveTab().Files.end())
                {
                    GetActiveTab().Files.erase(it);
                    LoadFiles();
                    if (App::GetInstance())
                    {
                        App::GetInstance()->SaveConfig();
                    }
                }
            }
            else if (cmd > 0)
            {
                CMINVOKECOMMANDINFO ci = {};
                ci.cbSize = sizeof(ci);
                ci.hwnd = hwnd;
                ci.lpVerb = MAKEINTRESOURCEA(cmd - 1);
                ci.nShow = SW_SHOWNORMAL;
                pContextMenu->InvokeCommand(&ci);
            }
        }
        DestroyMenu(hMenu);
    }

    pContextMenu->Release();
    pParentFolder->Release();
    CoTaskMemFree(pidlFull);
}

// ============================================================================
// CorralDropTarget - OLE IDropTarget implementation
// ============================================================================

CorralDropTarget::CorralDropTarget(CorralWindow *owner)
    : refCount(1), owner(owner), hasDropData(false) {}

HRESULT STDMETHODCALLTYPE CorralDropTarget::QueryInterface(REFIID riid, void **ppvObject)
{
    if (riid == IID_IUnknown || riid == IID_IDropTarget)
    {
        *ppvObject = static_cast<IDropTarget *>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CorralDropTarget::AddRef() { return InterlockedIncrement(&refCount); }
ULONG STDMETHODCALLTYPE CorralDropTarget::Release()
{
    LONG count = InterlockedDecrement(&refCount);
    if (count == 0)
        delete this;
    return count;
}

HRESULT STDMETHODCALLTYPE CorralDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    // Accept drops that have either CF_HDROP (files) or CFSTR_SHELLIDLIST (shell items)
    FORMATETC fmtHdrop = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC fmtShellIdList = {(CLIPFORMAT)RegisterClipboardFormatW(L"Shell IDList Array"), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};

    hasDropData = (pDataObj->QueryGetData(&fmtHdrop) == S_OK ||
                   pDataObj->QueryGetData(&fmtShellIdList) == S_OK);

    *pdwEffect = hasDropData ? DROPEFFECT_LINK : DROPEFFECT_NONE;
    LogCorralDrop(L"DragEnter: hasDropData=%d, effect=0x%X", hasDropData, *pdwEffect);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorralDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    if (!hasDropData)
    {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Show DROPEFFECT_COPY when hovering over an icon (drop-on-icon = open with)
    // Show DROPEFFECT_LINK when hovering over empty space (add to corral)
    POINT clientPt = {pt.x, pt.y};
    ScreenToClient(owner->hwnd, &clientPt);
    int hit = owner->HitTestIcon(clientPt.x, clientPt.y);
    *pdwEffect = (hit >= 0) ? DROPEFFECT_COPY : DROPEFFECT_LINK;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorralDropTarget::DragLeave()
{
    LogCorralDrop(L"DragLeave");
    hasDropData = false;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorralDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    // Check if dropped on a corral icon -> shell-execute (open with that icon)
    POINT clientPt = {pt.x, pt.y};
    ScreenToClient(owner->hwnd, &clientPt);
    int hit = owner->HitTestIcon(clientPt.x, clientPt.y);

    LogCorralDrop(L"Drop: screen=(%d,%d) client=(%d,%d) hitIcon=%d",
                  pt.x, pt.y, clientPt.x, clientPt.y, hit);

    if (hit >= 0)
    {
        *pdwEffect = DROPEFFECT_COPY;
        LogCorralDrop(L"Drop: on icon %d -> OnDropOnIcon", hit);
        owner->OnDropOnIcon(pDataObj, hit);
    }
    else
    {
        *pdwEffect = DROPEFFECT_LINK;
        LogCorralDrop(L"Drop: on empty space -> OnDrop (add to corral)");
        owner->OnDrop(pDataObj);
    }
    return S_OK;
}

// ============================================================================
// OnDrop - handles both regular files (CF_HDROP) and special shell items
// ============================================================================

void CorralWindow::OnDrop(IDataObject *pDataObj)
{
    if (GetActiveTab().IsVirtual)
        return;

    bool changed = false;
    std::wstring desktopPath = GetDesktopPath();
    std::wstring publicDesktopPath = GetPublicDesktopPath();

    // Try CF_HDROP first for files dragged from Explorer windows.
    // Note: the desktop shell resolves .lnk shortcuts in CF_HDROP (gives target path),
    // so we verify each file exists on the desktop. Resolved shortcuts are handled below
    // via CFSTR_SHELLIDLIST which preserves the actual .lnk filename.
    FORMATETC fmtHdrop = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stgHdrop = {};

    if (SUCCEEDED(pDataObj->GetData(&fmtHdrop, &stgHdrop)))
    {
        HDROP hDrop = (HDROP)GlobalLock(stgHdrop.hGlobal);
        if (hDrop)
        {
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < fileCount; i++)
            {
                wchar_t filePath[MAX_PATH];
                DragQueryFileW(hDrop, i, filePath, MAX_PATH);

                std::wstring fullPath(filePath);
                size_t lastSlash = fullPath.find_last_of(L"\\/");
                std::wstring fileName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

                // Verify the file actually exists on the desktop.
                // If not, CF_HDROP resolved a .lnk shortcut to its target - skip it
                // and let CFSTR_SHELLIDLIST handle it with the correct .lnk name.
                std::wstring userPath = desktopPath + L"\\" + fileName;
                std::wstring pubPath = publicDesktopPath + L"\\" + fileName;
                if (GetFileAttributesW(userPath.c_str()) == INVALID_FILE_ATTRIBUTES &&
                    GetFileAttributesW(pubPath.c_str()) == INVALID_FILE_ATTRIBUTES)
                {
                    continue;
                }

                std::string fileNameUtf8 = WideToUtf8(fileName);

                auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), fileNameUtf8);
                if (it == GetActiveTab().Files.end())
                {
                    GetActiveTab().Files.push_back(fileNameUtf8);
                    changed = true;

                    if (App::GetInstance())
                    {
                        App::GetInstance()->RemoveFileFromOtherCorrals(fileName, &GetActiveTab());
                    }
                }
            }
            GlobalUnlock(stgHdrop.hGlobal);
        }
        ReleaseStgMedium(&stgHdrop);
    }

    // Fallback: try CFSTR_SHELLIDLIST for special shell items (Recycle Bin, etc.)
    // and for .lnk shortcuts whose names CF_HDROP resolved to the target.
    if (!changed)
    {
        CLIPFORMAT cfShellIdList = (CLIPFORMAT)RegisterClipboardFormatW(L"Shell IDList Array");
        FORMATETC fmtShellIdList = {cfShellIdList, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stgShellIdList = {};

        if (SUCCEEDED(pDataObj->GetData(&fmtShellIdList, &stgShellIdList)))
        {
            CIDA *pida = (CIDA *)GlobalLock(stgShellIdList.hGlobal);
            if (pida && pida->cidl > 0)
            {
                LPCITEMIDLIST pidlParent = (LPCITEMIDLIST)((BYTE *)pida + pida->aoffset[0]);

                for (UINT i = 0; i < pida->cidl; i++)
                {
                    LPCITEMIDLIST pidlChild = (LPCITEMIDLIST)((BYTE *)pida + pida->aoffset[i + 1]);

                    LPITEMIDLIST pidlAbsolute = ILCombine(pidlParent, pidlChild);
                    if (!pidlAbsolute)
                        continue;

                    std::string entryUtf8;

                    // Try SIGDN_FILESYSPATH first - this gives the actual filesystem
                    // path (e.g. "C:\Users\...\Desktop\DexCorral-Register.lnk")
                    // without resolving through shortcuts like other SIGDN flags do.
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(SHGetNameFromIDList(pidlAbsolute, SIGDN_FILESYSPATH, &pszPath)) && pszPath)
                    {
                        std::wstring fsPath = pszPath;
                        size_t lastSlash = fsPath.find_last_of(L"\\/");
                        std::wstring fileName = (lastSlash != std::wstring::npos) ? fsPath.substr(lastSlash + 1) : fsPath;
                        if (!fileName.empty())
                        {
                            entryUtf8 = WideToUtf8(fileName);
                        }
                        CoTaskMemFree(pszPath);
                    }
                    else
                    {
                        // No filesystem path - check for special shell item (::CLSID)
                        PWSTR pszParsing = nullptr;
                        if (SUCCEEDED(SHGetNameFromIDList(pidlAbsolute, SIGDN_DESKTOPABSOLUTEPARSING, &pszParsing)) && pszParsing)
                        {
                            if (pszParsing[0] == L':' && pszParsing[1] == L':')
                            {
                                std::wstring clsid = pszParsing + 2;
                                entryUtf8 = "shell:" + WideToUtf8(clsid);
                            }
                            CoTaskMemFree(pszParsing);
                        }
                    }

                    if (!entryUtf8.empty())
                    {
                        auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), entryUtf8);
                        if (it == GetActiveTab().Files.end())
                        {
                            GetActiveTab().Files.push_back(entryUtf8);
                            changed = true;

                            if (App::GetInstance())
                            {
                                std::wstring wName = Utf8ToWide(entryUtf8);
                                App::GetInstance()->RemoveFileFromOtherCorrals(wName, &GetActiveTab());
                            }
                        }
                    }

                    CoTaskMemFree(pidlAbsolute);
                }
            }
            GlobalUnlock(stgShellIdList.hGlobal);
            ReleaseStgMedium(&stgShellIdList);
        }
    }

    if (changed)
    {
        LoadFiles();
        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// OnDropOnIcon - drop a file onto a corral icon (shell-execute / open with)
// ============================================================================

void CorralWindow::OnDropOnIcon(IDataObject *pDataObj, int iconIndex)
{
    if (iconIndex < 0 || iconIndex >= (int)icons.size())
        return;

    const auto &targetIcon = icons[iconIndex];

    // Get the target icon's IDropTarget via the shell and forward the drop.
    // This replicates Explorer's behavior: dropping onto an exe opens the file,
    // dropping onto a folder moves/copies the file, dropping onto a shortcut
    // invokes the shortcut's target with the dropped file, etc.
    std::wstring targetPath;
    if (targetIcon.isSpecialIcon)
    {
        targetPath = L"::" + targetIcon.clsid;
    }
    else
    {
        targetPath = targetIcon.fullPath;
    }

    LogCorralDrop(L"OnDropOnIcon: icon=%d target='%s' special=%d",
                  iconIndex, targetPath.c_str(), targetIcon.isSpecialIcon);

    IShellItem *pItem = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(targetPath.c_str(), nullptr, IID_PPV_ARGS(&pItem));
    if (SUCCEEDED(hr) && pItem)
    {
        IDropTarget *pDropTarget = nullptr;
        hr = pItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pDropTarget));
        if (SUCCEEDED(hr) && pDropTarget)
        {
            LogCorralDrop(L"OnDropOnIcon: got IDropTarget, forwarding drop");
            POINTL ptl = {0, 0};
            DWORD dwEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
            pDropTarget->DragEnter(pDataObj, MK_LBUTTON, ptl, &dwEffect);
            dwEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
            pDropTarget->Drop(pDataObj, MK_LBUTTON, ptl, &dwEffect);
            pDropTarget->Release();
            LogCorralDrop(L"OnDropOnIcon: drop forwarded, effect=0x%X", dwEffect);
        }
        else
        {
            LogCorralDrop(L"OnDropOnIcon: BindToHandler failed hr=0x%08X", hr);
        }
        pItem->Release();
    }
    else
    {
        LogCorralDrop(L"OnDropOnIcon: SHCreateItemFromParsingName failed hr=0x%08X", hr);
    }
}

// ============================================================================
// Icon reordering
// ============================================================================

void CorralWindow::OnIconDrag(int x, int y)
{
    if (!isDraggingIcon || draggedIconIndex < 0)
        return;

    // Find which icon slot we're over
    int newDropTarget = -1;
    POINT pt = {x, y};

    for (int i = 0; i < (int)icons.size(); i++)
    {
        if (PtInRect(&icons[i].rect, pt))
        {
            newDropTarget = i;
            break;
        }
    }

    if (newDropTarget != dropTargetIndex)
    {
        dropTargetIndex = newDropTarget;
        UpdateLayeredContent();
    }

    // Track if we're outside this corral (for drag-out detection)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    iconDragOutside = !PtInRect(&clientRect, pt);
}

void CorralWindow::OnIconDragEnd()
{
    if (!isDraggingIcon || draggedIconIndex < 0 || draggedIconIndex >= (int)GetActiveTab().Files.size())
    {
        isDraggingIcon = false;
        draggedIconIndex = -1;
        dropTargetIndex = -1;
        iconDragOutside = false;
        return;
    }

    std::string draggedFile = GetActiveTab().Files[draggedIconIndex];

    // Check if dragged outside the corral
    if (iconDragOutside)
    {
        POINT screenPt;
        GetCursorPos(&screenPt);

        // Check if dropped on another corral
        App *app = App::GetInstance();
        CorralWindow *targetCorral = nullptr;

        if (app)
        {
            for (const auto &other : app->GetCorrals())
            {
                if (other.get() == this)
                    continue;

                RECT otherRect;
                GetWindowRect(other->GetHWND(), &otherRect);
                if (PtInRect(&otherRect, screenPt))
                {
                    targetCorral = other.get();
                    break;
                }
            }
        }

        if (targetCorral)
        {
            // Move to another corral
            GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);
            targetCorral->AddFile(draggedFile);
            LoadFiles();
            if (app)
            {
                app->SaveConfig();
            }
        }
        else
        {
            // Dropped outside all corrals - remove from this corral (back to desktop)
            GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);
            LoadFiles();
            if (app)
            {
                app->SaveConfig();
                app->UpdateHookHiddenIcons();

                // Position the icon at the drop location on the desktop
                std::wstring displayName;
                if (CorralWindow::IsSpecialIconEntry(draggedFile))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(draggedFile);
                    displayName = DesktopIcons::GetSpecialIconDisplayName(clsid);
                }
                else
                {
                    displayName = IconUtils::StripLnkExtension(Utf8ToWide(draggedFile));
                }

                if (!displayName.empty())
                {
                    // Convert screen coords to desktop ListView client coords
                    HWND hListView = DesktopIcons::GetDesktopListView();
                    if (hListView)
                    {
                        POINT clientPt = screenPt;
                        ScreenToClient(hListView, &clientPt);
                        DesktopIcons::PositionIcon(displayName, clientPt.x, clientPt.y);
                    }
                }
            }
        }
    }
    else if (dropTargetIndex >= 0 && dropTargetIndex != draggedIconIndex && dropTargetIndex < (int)GetActiveTab().Files.size())
    {
        // Reorder within corral
        GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);

        int insertAt = dropTargetIndex;
        if (draggedIconIndex < dropTargetIndex)
        {
            insertAt--;
        }
        if (insertAt < 0)
            insertAt = 0;
        if (insertAt > (int)GetActiveTab().Files.size())
            insertAt = (int)GetActiveTab().Files.size();

        GetActiveTab().Files.insert(GetActiveTab().Files.begin() + insertAt, draggedFile);

        LoadFiles();
        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
    }

    isDraggingIcon = false;
    draggedIconIndex = -1;
    dropTargetIndex = -1;
    iconDragOutside = false;
    UpdateLayeredContent();
}
