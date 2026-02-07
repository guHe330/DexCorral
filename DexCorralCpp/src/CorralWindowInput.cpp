// CorralWindowInput.cpp - Mouse handling, scrollbar, resize, snap, file ops
#include "CorralWindow.h"
#include "App.h"
#include <windowsx.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <algorithm>
#include <cmath>

// ============================================================================
// Scrollbar support
// ============================================================================

bool CorralWindow::NeedsScrollbar() const {
    return contentHeight > GetVisibleHeight();
}

int CorralWindow::GetContentHeight() const {
    return contentHeight;
}

int CorralWindow::GetVisibleHeight() const {
    RECT rect;
    GetClientRect(hwnd, &rect);
    return rect.bottom - ICON_AREA_TOP;
}

RECT CorralWindow::GetScrollbarTrackRect() const {
    RECT rect;
    GetClientRect(hwnd, &rect);
    return {
        rect.right - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN,
        ICON_AREA_TOP + SCROLLBAR_MARGIN,
        rect.right - SCROLLBAR_MARGIN,
        rect.bottom - SCROLLBAR_MARGIN
    };
}

RECT CorralWindow::GetScrollbarThumbRect() const {
    if (!NeedsScrollbar()) return { 0, 0, 0, 0 };

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();

    // Thumb size proportional to visible/content ratio
    int thumbHeight = (std::max)(SCROLLBAR_MIN_THUMB,
        (int)((float)visibleHeight / contentHeight * trackHeight));

    // Thumb position based on scroll position
    int scrollRange = contentHeight - visibleHeight;
    int thumbRange = trackHeight - thumbHeight;
    int thumbTop = track.top;
    if (scrollRange > 0) {
        thumbTop = track.top + (int)((float)scrollPosition / scrollRange * thumbRange);
    }

    return {
        track.left,
        thumbTop,
        track.right,
        thumbTop + thumbHeight
    };
}

bool CorralWindow::HitTestScrollbar(int x, int y) const {
    if (!NeedsScrollbar()) return false;
    RECT track = GetScrollbarTrackRect();
    POINT pt = { x, y };
    return PtInRect(&track, pt) != FALSE;
}

bool CorralWindow::HitTestScrollbarThumb(int x, int y) const {
    if (!NeedsScrollbar()) return false;
    RECT thumb = GetScrollbarThumbRect();
    POINT pt = { x, y };
    return PtInRect(&thumb, pt) != FALSE;
}

void CorralWindow::OnMouseWheel(int delta) {
    // Only scroll if content exceeds visible area
    if (!NeedsScrollbar()) return;

    // Scroll proportionally - multiply first to avoid integer truncation for small deltas
    // (precision touchpads send small incremental deltas like 6, 20, etc. instead of 120)
    int scrollAmount = delta * iconSpacingY / WHEEL_DELTA;
    scrollPosition -= scrollAmount;
    ClampScrollPosition();
    UpdateLayeredContent();
}

void CorralWindow::StartScrollbarDrag(int y) {
    isDraggingScrollbar = true;
    scrollbarDragStartY = y;
    scrollbarDragStartPos = scrollPosition;
    SetCapture(hwnd);
}

void CorralWindow::DoScrollbarDrag(int y) {
    if (!isDraggingScrollbar) return;

    RECT track = GetScrollbarTrackRect();
    int trackHeight = track.bottom - track.top;
    int visibleHeight = GetVisibleHeight();
    int thumbHeight = (std::max)(SCROLLBAR_MIN_THUMB,
        (int)((float)visibleHeight / contentHeight * trackHeight));
    int thumbRange = trackHeight - thumbHeight;
    int scrollRange = contentHeight - visibleHeight;

    if (thumbRange > 0 && scrollRange > 0) {
        int deltaY = y - scrollbarDragStartY;
        int newScrollPos = scrollbarDragStartPos + (int)((float)deltaY / thumbRange * scrollRange);
        scrollPosition = newScrollPos;
        ClampScrollPosition();
        UpdateLayeredContent();
    }
}

void CorralWindow::EndScrollbarDrag() {
    if (isDraggingScrollbar) {
        isDraggingScrollbar = false;
        ReleaseCapture();
    }
}

void CorralWindow::ClampScrollPosition() {
    int maxScroll = contentHeight - GetVisibleHeight();
    if (maxScroll < 0) maxScroll = 0;
    if (scrollPosition < 0) scrollPosition = 0;
    if (scrollPosition > maxScroll) scrollPosition = maxScroll;
}

// ============================================================================
// Resize support
// ============================================================================

int CorralWindow::HitTestResize(int x, int y) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    bool nearLeft = (x <= RESIZE_BORDER);
    bool nearRight = (x >= rect.right - RESIZE_BORDER);
    bool nearTop = (y <= RESIZE_BORDER && y > 0);  // Don't conflict with title bar at y=0
    bool nearBottom = (y >= rect.bottom - RESIZE_BORDER);

    // When rolled up (minimized), disable resizing completely
    if (config.IsRolledUp) {
        return 0;
    }

    // Corners first (they take priority)
    if (nearLeft && nearTop) return HTTOPLEFT;
    if (nearRight && nearTop) return HTTOPRIGHT;
    if (nearLeft && nearBottom) return HTBOTTOMLEFT;
    if (nearRight && nearBottom) return HTBOTTOMRIGHT;

    // Then edges
    if (nearLeft) return HTLEFT;
    if (nearRight) return HTRIGHT;
    if (nearTop) return HTTOP;
    if (nearBottom) return HTBOTTOM;

    return 0;
}

void CorralWindow::StartResize(int hitTest, int x, int y) {
    isResizing = true;
    resizeMode = hitTest;
    GetCursorPos(&resizeStart);
    GetWindowRect(hwnd, &resizeStartRect);
    SetCapture(hwnd);
}

void CorralWindow::DoResize(int screenX, int screenY) {
    if (!isResizing) return;

    int dx = screenX - resizeStart.x;
    int dy = screenY - resizeStart.y;

    int newLeft = resizeStartRect.left;
    int newTop = resizeStartRect.top;
    int newWidth = resizeStartRect.right - resizeStartRect.left;
    int newHeight = resizeStartRect.bottom - resizeStartRect.top;

    // Handle horizontal resizing
    if (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT) {
        newLeft += dx;
        newWidth -= dx;
    }
    if (resizeMode == HTRIGHT || resizeMode == HTTOPRIGHT || resizeMode == HTBOTTOMRIGHT) {
        newWidth += dx;
    }

    // Handle vertical resizing
    if (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT) {
        newTop += dy;
        newHeight -= dy;
    }
    if (resizeMode == HTBOTTOM || resizeMode == HTBOTTOMLEFT || resizeMode == HTBOTTOMRIGHT) {
        newHeight += dy;
    }

    // Minimum size constraints
    if (newWidth < 100) {
        if (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT) {
            newLeft = resizeStartRect.right - 100;
        }
        newWidth = 100;
    }
    if (newHeight < 80) {
        if (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT) {
            newTop = resizeStartRect.bottom - 80;
        }
        newHeight = 80;
    }

    // Apply snap to resize unless Shift is held
    if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
        ApplyResizeSnap(newLeft, newTop, newWidth, newHeight, resizeMode);
    }

    SetWindowPos(hwnd, nullptr, newLeft, newTop, newWidth, newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void CorralWindow::EndResize() {
    if (isResizing) {
        isResizing = false;
        ReleaseCapture();
        SyncConfigFromWindow();
        CalculateIconLayout();
        UpdateLayeredContent();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Snap support
// ============================================================================

void CorralWindow::ApplySnap(int& newLeft, int& newTop, int width, int height) {
    App* app = App::GetInstance();
    if (!app) return;

    int newRight = newLeft + width;
    int newBottom = newTop + height;

    // Snap to screen edges
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Left edge of screen
    if (std::abs(newLeft) < SNAP_DISTANCE) {
        newLeft = SNAP_GAP;
    }
    // Right edge of screen
    if (std::abs(newRight - screenWidth) < SNAP_DISTANCE) {
        newLeft = screenWidth - width - SNAP_GAP;
    }
    // Top edge of screen
    if (std::abs(newTop) < SNAP_DISTANCE) {
        newTop = SNAP_GAP;
    }
    // Bottom edge of screen (account for taskbar ~40px)
    if (std::abs(newBottom - screenHeight + 40) < SNAP_DISTANCE) {
        newTop = screenHeight - height - 40 - SNAP_GAP;
    }

    // Snap to other corrals
    const auto& corrals = app->GetCorrals();
    for (const auto& other : corrals) {
        if (other->GetHWND() == hwnd) continue;  // Skip self

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        int otherLeft = otherRect.left;
        int otherTop = otherRect.top;
        int otherRight = otherRect.right;
        int otherBottom = otherRect.bottom;

        // Check vertical alignment (are we in the same vertical band?)
        bool verticalOverlap = (newTop < otherBottom + SNAP_DISTANCE) && (newBottom > otherTop - SNAP_DISTANCE);
        // Check horizontal alignment (are we in the same horizontal band?)
        bool horizontalOverlap = (newLeft < otherRight + SNAP_DISTANCE) && (newRight > otherLeft - SNAP_DISTANCE);

        if (verticalOverlap) {
            // Snap our right edge to their left edge (with gap)
            if (std::abs(newRight - otherLeft + SNAP_GAP) < SNAP_DISTANCE) {
                newLeft = otherLeft - width - SNAP_GAP;
            }
            // Snap our left edge to their right edge (with gap)
            if (std::abs(newLeft - otherRight - SNAP_GAP) < SNAP_DISTANCE) {
                newLeft = otherRight + SNAP_GAP;
            }
            // Snap our left edge to their left edge (align)
            if (std::abs(newLeft - otherLeft) < SNAP_DISTANCE) {
                newLeft = otherLeft;
            }
            // Snap our right edge to their right edge (align)
            if (std::abs(newRight - otherRight) < SNAP_DISTANCE) {
                newLeft = otherRight - width;
            }
        }

        if (horizontalOverlap) {
            // Snap our bottom edge to their top edge (with gap)
            if (std::abs(newBottom - otherTop + SNAP_GAP) < SNAP_DISTANCE) {
                newTop = otherTop - height - SNAP_GAP;
            }
            // Snap our top edge to their bottom edge (with gap)
            if (std::abs(newTop - otherBottom - SNAP_GAP) < SNAP_DISTANCE) {
                newTop = otherBottom + SNAP_GAP;
            }
            // Snap our top edge to their top edge (align)
            if (std::abs(newTop - otherTop) < SNAP_DISTANCE) {
                newTop = otherTop;
            }
            // Snap our bottom edge to their bottom edge (align)
            if (std::abs(newBottom - otherBottom) < SNAP_DISTANCE) {
                newTop = otherBottom - height;
            }
        }
    }
}

void CorralWindow::ApplyResizeSnap(int& newLeft, int& newTop, int& newWidth, int& newHeight, int resizeMode) {
    App* app = App::GetInstance();
    if (!app) return;

    // Determine which edges are being resized
    bool resizingLeft = (resizeMode == HTLEFT || resizeMode == HTTOPLEFT || resizeMode == HTBOTTOMLEFT);
    bool resizingRight = (resizeMode == HTRIGHT || resizeMode == HTTOPRIGHT || resizeMode == HTBOTTOMRIGHT);
    bool resizingTop = (resizeMode == HTTOP || resizeMode == HTTOPLEFT || resizeMode == HTTOPRIGHT);
    bool resizingBottom = (resizeMode == HTBOTTOM || resizeMode == HTBOTTOMLEFT || resizeMode == HTBOTTOMRIGHT);

    int newRight = newLeft + newWidth;
    int newBottom = newTop + newHeight;

    // Snap to screen edges
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Left edge of screen
    if (resizingLeft && std::abs(newLeft - SNAP_GAP) < SNAP_DISTANCE) {
        int oldRight = newRight;
        newLeft = SNAP_GAP;
        newWidth = oldRight - newLeft;
    }
    // Right edge of screen
    if (resizingRight && std::abs(newRight - screenWidth + SNAP_GAP) < SNAP_DISTANCE) {
        newWidth = screenWidth - newLeft - SNAP_GAP;
    }
    // Top edge of screen
    if (resizingTop && std::abs(newTop - SNAP_GAP) < SNAP_DISTANCE) {
        int oldBottom = newBottom;
        newTop = SNAP_GAP;
        newHeight = oldBottom - newTop;
    }
    // Bottom edge of screen (account for taskbar ~40px)
    if (resizingBottom && std::abs(newBottom - screenHeight + 40 + SNAP_GAP) < SNAP_DISTANCE) {
        newHeight = screenHeight - newTop - 40 - SNAP_GAP;
    }

    // Recalculate edges after screen snap
    newRight = newLeft + newWidth;
    newBottom = newTop + newHeight;

    // Snap to other corrals
    const auto& corrals = app->GetCorrals();
    for (const auto& other : corrals) {
        if (other->GetHWND() == hwnd) continue;

        RECT otherRect;
        GetWindowRect(other->GetHWND(), &otherRect);

        // Check vertical overlap
        bool verticalOverlap = (newTop < otherRect.bottom + SNAP_DISTANCE) && (newBottom > otherRect.top - SNAP_DISTANCE);
        // Check horizontal overlap
        bool horizontalOverlap = (newLeft < otherRect.right + SNAP_DISTANCE) && (newRight > otherRect.left - SNAP_DISTANCE);

        if (verticalOverlap) {
            if (resizingLeft) {
                // Snap left edge to their right edge (with gap)
                if (std::abs(newLeft - otherRect.right - SNAP_GAP) < SNAP_DISTANCE) {
                    int oldRight = newRight;
                    newLeft = otherRect.right + SNAP_GAP;
                    newWidth = oldRight - newLeft;
                }
                // Snap left edge to their left edge (align)
                if (std::abs(newLeft - otherRect.left) < SNAP_DISTANCE) {
                    int oldRight = newRight;
                    newLeft = otherRect.left;
                    newWidth = oldRight - newLeft;
                }
            }
            if (resizingRight) {
                // Snap right edge to their left edge (with gap)
                if (std::abs(newRight - otherRect.left + SNAP_GAP) < SNAP_DISTANCE) {
                    newWidth = otherRect.left - newLeft - SNAP_GAP;
                }
                // Snap right edge to their right edge (align)
                if (std::abs(newRight - otherRect.right) < SNAP_DISTANCE) {
                    newWidth = otherRect.right - newLeft;
                }
            }
        }

        if (horizontalOverlap) {
            if (resizingTop) {
                // Snap top edge to their bottom edge (with gap)
                if (std::abs(newTop - otherRect.bottom - SNAP_GAP) < SNAP_DISTANCE) {
                    int oldBottom = newBottom;
                    newTop = otherRect.bottom + SNAP_GAP;
                    newHeight = oldBottom - newTop;
                }
                // Snap top edge to their top edge (align)
                if (std::abs(newTop - otherRect.top) < SNAP_DISTANCE) {
                    int oldBottom = newBottom;
                    newTop = otherRect.top;
                    newHeight = oldBottom - newTop;
                }
            }
            if (resizingBottom) {
                // Snap bottom edge to their top edge (with gap)
                if (std::abs(newBottom - otherRect.top + SNAP_GAP) < SNAP_DISTANCE) {
                    newHeight = otherRect.top - newTop - SNAP_GAP;
                }
                // Snap bottom edge to their bottom edge (align)
                if (std::abs(newBottom - otherRect.bottom) < SNAP_DISTANCE) {
                    newHeight = otherRect.bottom - newTop;
                }
            }
        }

        // Recalculate after each corral snap for next iteration
        newRight = newLeft + newWidth;
        newBottom = newTop + newHeight;
    }

    // Enforce minimum size (adjusting position for left/top edge resizing)
    if (newWidth < 100) {
        if (resizingLeft) {
            newLeft -= (100 - newWidth);
        }
        newWidth = 100;
    }
    if (newHeight < 80) {
        if (resizingTop) {
            newTop -= (80 - newHeight);
        }
        newHeight = 80;
    }
}

// ============================================================================
// Mouse event handlers
// ============================================================================

void CorralWindow::OnLeftButtonDown(int x, int y) {
    // Check resize area first (but not when rolled up)
    if (!config.IsRolledUp) {
        int resizeHit = HitTestResize(x, y);
        if (resizeHit) {
            StartResize(resizeHit, x, y);
            return;
        }
    }

    // Check if clicked on scrollbar
    if (HitTestScrollbar(x, y)) {
        if (HitTestScrollbarThumb(x, y)) {
            // Clicked on thumb - start dragging
            StartScrollbarDrag(y);
        } else {
            // Clicked in track - jump to position
            RECT track = GetScrollbarTrackRect();
            RECT thumb = GetScrollbarThumbRect();
            int thumbHeight = thumb.bottom - thumb.top;
            int trackHeight = track.bottom - track.top;
            int thumbRange = trackHeight - thumbHeight;
            int scrollRange = contentHeight - GetVisibleHeight();

            // Calculate new scroll position - center thumb on click point
            int clickOffset = y - track.top - (thumbHeight / 2);
            if (clickOffset < 0) clickOffset = 0;
            if (clickOffset > thumbRange) clickOffset = thumbRange;

            if (thumbRange > 0) {
                scrollPosition = (int)((float)clickOffset / thumbRange * scrollRange);
                ClampScrollPosition();
                UpdateLayeredContent();
            }
        }
        return;
    }

    // Check if clicked on a tab in title bar
    int tabHit = HitTestTab(x, y);
    if (tabHit >= 0) {
        if (tabHit != config.ActiveTabIndex) {
            SetActiveTab(tabHit);
        }
        // Allow dragging window from tab
        isDragging = true;
        GetCursorPos(&dragStart);
        GetWindowRect(hwnd, &dragStartRect);
        SetCapture(hwnd);
        return;
    }

    // Check if clicked on an icon - select it (drag starts on mouse move)
    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        // Check if we clicked on the label of an already-selected icon
        if (hit == selectedIcon && HitTestIconLabel(x, y, hit)) {
            // Clicked on label of selected icon - start rename
            StartIconRename(hit);
            return;
        }

        // Otherwise, just select the icon
        selectedIcon = hit;
        draggedIconIndex = hit;  // Potential drag candidate
        iconDragStart = { x, y };  // Store starting position
        dropTargetIndex = -1;
        SetFocus(hwnd);  // Take keyboard focus so F2 works
        SetCapture(hwnd);
        UpdateLayeredContent();  // Show selection highlight
        return;
    }

    // Deselect
    if (selectedIcon >= 0) {
        selectedIcon = -1;
        UpdateLayeredContent();
    }

    // Title bar - start dragging window (empty area)
    if (y < TITLE_BAR_HEIGHT) {
        isDragging = true;
        GetCursorPos(&dragStart);
        GetWindowRect(hwnd, &dragStartRect);
        SetCapture(hwnd);
    }
}

void CorralWindow::OnLeftButtonDblClick(int x, int y) {
    // Double-click on title bar toggles roll-up
    if (y < TITLE_BAR_HEIGHT) {
        ToggleRollUp();
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        OpenFile(hit);
    }
}

void CorralWindow::OnLeftButtonUp(int x, int y) {
    if (isResizing) {
        EndResize();
    }
    else if (isDraggingScrollbar) {
        EndScrollbarDrag();
    }
    else if (isDraggingIcon) {
        OnIconDragEnd();
        ReleaseCapture();
    }
    else if (draggedIconIndex >= 0) {
        // Mouse up without dragging - just a selection click
        draggedIconIndex = -1;
        ReleaseCapture();
    }
    else if (isDragging) {
        isDragging = false;
        ReleaseCapture();

        // Check for merge with another corral
        POINT pt;
        GetCursorPos(&pt);
        App* app = App::GetInstance();
        if (app) {
            for (const auto& other : app->GetCorrals()) {
                if (other.get() == this) continue;

                RECT otherRect;
                GetWindowRect(other->GetHWND(), &otherRect);

                // If dropped on another window's title bar, merge
                if (PtInRect(&otherRect, pt) && (pt.y - otherRect.top < TITLE_BAR_HEIGHT)) {
                    MergeWith(other.get());
                    return; // This window is destroyed now
                }
            }
        }

        SyncConfigFromWindow();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

void CorralWindow::OnRightButtonDown(int x, int y) {
    // Check title bar first - don't let scrolled icons consume clicks here
    if (y < TITLE_BAR_HEIGHT) {
        ShowContextMenu(x, y);
        return;
    }

    int hit = HitTestIcon(x, y);
    if (hit >= 0) {
        selectedIcon = hit;
        UpdateLayeredContent();

        POINT pt = { x, y };
        ClientToScreen(hwnd, &pt);
        ShowShellContextMenu(hit, pt.x, pt.y);
        return;
    }

    ShowContextMenu(x, y);
}

// ============================================================================
// File operations
// ============================================================================

void CorralWindow::OpenFile(int iconIndex) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) return;

    const auto& icon = icons[iconIndex];

    if (icon.isSpecialIcon) {
        // Open special shell item via PIDL
        std::wstring parseName = L"::" + icon.clsid;
        LPITEMIDLIST pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)) && pidl) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
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

void CorralWindow::ShowShellContextMenu(int iconIndex, int screenX, int screenY) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) return;

    const auto& icon = icons[iconIndex];

    LPITEMIDLIST pidlFull = nullptr;
    HRESULT hr;
    if (icon.isSpecialIcon) {
        std::wstring parseName = L"::" + icon.clsid;
        hr = SHParseDisplayName(parseName.c_str(), nullptr, &pidlFull, 0, nullptr);
    } else {
        hr = SHParseDisplayName(icon.fullPath.c_str(), nullptr, &pidlFull, 0, nullptr);
    }
    if (FAILED(hr) || !pidlFull) return;

    IShellFolder* pParentFolder = nullptr;
    LPCITEMIDLIST pidlChild = nullptr;
    hr = SHBindToParent(pidlFull, IID_IShellFolder, (void**)&pParentFolder, &pidlChild);
    if (FAILED(hr) || !pParentFolder) {
        CoTaskMemFree(pidlFull);
        return;
    }

    IContextMenu* pContextMenu = nullptr;
    hr = pParentFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, nullptr, (void**)&pContextMenu);
    if (FAILED(hr) || !pContextMenu) {
        pParentFolder->Release();
        CoTaskMemFree(pidlFull);
        return;
    }

    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        hr = pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL | CMF_EXPLORE);
        if (SUCCEEDED(hr)) {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 0x7FFF + 1, L"Remove from Corral");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                screenX, screenY, 0, hwnd, nullptr);
            PostMessageW(hwnd, WM_NULL, 0, 0);

            if (cmd == 0x7FFF + 1) {
                auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), icon.fileName);
                if (it != GetActiveTab().Files.end()) {
                    GetActiveTab().Files.erase(it);
                    LoadFiles();
                    if (App::GetInstance()) {
                        App::GetInstance()->SaveConfig();
                    }
                }
            }
            else if (cmd > 0) {
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

void CorralWindow::OnDropFiles(HDROP hDrop) {
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

    bool changed = false;
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t filePath[MAX_PATH];
        DragQueryFileW(hDrop, i, filePath, MAX_PATH);

        std::wstring fullPath(filePath);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        std::wstring fileName = (lastSlash != std::wstring::npos) ?
            fullPath.substr(lastSlash + 1) : fullPath;

        int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string fileNameUtf8(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameUtf8[0], size, nullptr, nullptr);

        auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), fileNameUtf8);
        if (it == GetActiveTab().Files.end()) {
            GetActiveTab().Files.push_back(fileNameUtf8);
            changed = true;

            if (App::GetInstance()) {
                App::GetInstance()->RemoveFileFromOtherCorrals(fileName, &GetActiveTab());
            }
        }
    }

    DragFinish(hDrop);

    if (changed) {
        LoadFiles();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Icon reordering
// ============================================================================

void CorralWindow::OnIconDrag(int x, int y) {
    if (!isDraggingIcon || draggedIconIndex < 0) return;

    // Find which icon slot we're over
    int newDropTarget = -1;
    POINT pt = { x, y };

    for (int i = 0; i < (int)icons.size(); i++) {
        if (PtInRect(&icons[i].rect, pt)) {
            newDropTarget = i;
            break;
        }
    }

    if (newDropTarget != dropTargetIndex) {
        dropTargetIndex = newDropTarget;
        UpdateLayeredContent();
    }

    // Track if we're outside this corral (for drag-out detection)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    iconDragOutside = !PtInRect(&clientRect, pt);
}

void CorralWindow::OnIconDragEnd() {
    if (!isDraggingIcon || draggedIconIndex < 0 || draggedIconIndex >= (int)GetActiveTab().Files.size()) {
        isDraggingIcon = false;
        draggedIconIndex = -1;
        dropTargetIndex = -1;
        iconDragOutside = false;
        return;
    }

    std::string draggedFile = GetActiveTab().Files[draggedIconIndex];

    // Check if dragged outside the corral
    if (iconDragOutside) {
        POINT screenPt;
        GetCursorPos(&screenPt);

        // Check if dropped on another corral
        App* app = App::GetInstance();
        CorralWindow* targetCorral = nullptr;

        if (app) {
            for (const auto& other : app->GetCorrals()) {
                if (other.get() == this) continue;

                RECT otherRect;
                GetWindowRect(other->GetHWND(), &otherRect);
                if (PtInRect(&otherRect, screenPt)) {
                    targetCorral = other.get();
                    break;
                }
            }
        }

        if (targetCorral) {
            // Move to another corral
            GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);
            targetCorral->AddFile(draggedFile);
            LoadFiles();
            if (app) {
                app->SaveConfig();
            }
        }
        else {
            // Dropped outside all corrals - remove from this corral (back to desktop)
            GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);
            LoadFiles();
            if (app) {
                app->SaveConfig();
            }
        }
    }
    else if (dropTargetIndex >= 0 && dropTargetIndex != draggedIconIndex && dropTargetIndex < (int)GetActiveTab().Files.size()) {
        // Reorder within corral
        GetActiveTab().Files.erase(GetActiveTab().Files.begin() + draggedIconIndex);

        int insertAt = dropTargetIndex;
        if (draggedIconIndex < dropTargetIndex) {
            insertAt--;
        }
        if (insertAt < 0) insertAt = 0;
        if (insertAt > (int)GetActiveTab().Files.size()) insertAt = (int)GetActiveTab().Files.size();

        GetActiveTab().Files.insert(GetActiveTab().Files.begin() + insertAt, draggedFile);

        LoadFiles();
        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }

    isDraggingIcon = false;
    draggedIconIndex = -1;
    dropTargetIndex = -1;
    iconDragOutside = false;
    UpdateLayeredContent();
}
