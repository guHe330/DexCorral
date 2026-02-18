/**
 * CorralWindowIcons.cpp - Icon loading, caching, layout, and rendering
 *
 * Manages icon extraction from files, caching for performance, grid layout calculation,
 * hit testing for mouse interactions, and per-icon rendering. Detects icon content
 * (full vs padded), handles special cases like shortcuts and OneDrive cloud files,
 * and provides visual selection/hover states.
 */

#include "CorralWindow.h"
#include "Constants.h"
#include "FolderWatcher.h"
#include <shellapi.h>
#include <ShlObj.h>
#include <commoncontrols.h>
#include <algorithm>

// Cloud file attributes (defined in Windows 10 SDK 1709+, provide fallbacks)
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif
#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED 0x00080000
#endif
#ifndef FILE_ATTRIBUTE_UNPINNED
#define FILE_ATTRIBUTE_UNPINNED 0x00100000
#endif

// ============================================================================
// High-resolution icon extraction via system image list
// ============================================================================

// Get the appropriate SHIL_ constant for a given icon size
static int GetImageListType(int iconSize) {
    if (iconSize <= 16) return SHIL_SMALL;       // 16x16
    if (iconSize <= 32) return SHIL_LARGE;        // 32x32
    if (iconSize <= 48) return SHIL_EXTRALARGE;   // 48x48
    return SHIL_JUMBO;                            // 256x256
}

/**
 * Checks if an icon has actual content filling its canvas.
 *
 * Some applications only provide 32x32 icons, so requesting larger sizes (SHIL_EXTRALARGE,
 * SHIL_JUMBO) returns a canvas with the small icon centered and surrounded by transparent
 * padding. This function detects padded icons by checking if the non-transparent pixels
 * fill at least 70% of both dimensions.
 *
 * Returns true if the icon content fills at least 70% threshold in both dimensions,
 * false if padding is detected (meaning we should fall back to a smaller icon).
 */
static bool IconHasFullContent(HICON hIcon, int expectedSize) {
    if (!hIcon || expectedSize <= 32) return true;  // No need to check small icons

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = expectedSize;
    bmi.bmiHeader.biHeight = -expectedSize;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pixels = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, (void**)&pixels, nullptr, 0);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return true;
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
    memset(pixels, 0, expectedSize * expectedSize * 4);
    DrawIconEx(memDC, 0, 0, hIcon, expectedSize, expectedSize, 0, nullptr, DI_NORMAL);

    // Find bounding box of non-transparent pixels
    int minX = expectedSize, minY = expectedSize, maxX = 0, maxY = 0;
    for (int y = 0; y < expectedSize; y++) {
        for (int x = 0; x < expectedSize; x++) {
            BYTE alpha = (pixels[y * expectedSize + x] >> 24) & 0xFF;
            if (alpha > 10) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    if (maxX <= minX || maxY <= minY) return false;  // Empty icon

    int contentW = maxX - minX + 1;
    int contentH = maxY - minY + 1;
    // Content should fill at least 70% of the canvas
    return (contentW >= (int)(expectedSize * ICON_CONTENT_THRESHOLD) && contentH >= (int)(expectedSize * ICON_CONTENT_THRESHOLD));
}

// Extract icon from system image list at a given index and size.
// If the icon has too much padding (app lacks high-res icon), falls back to
// SHIL_LARGE (32x32) which DrawIconEx will scale up.
static HICON ExtractFromImageList(int iconIndex, int iconSize) {
    int imageListType = GetImageListType(iconSize);

    IImageList* pImageList = nullptr;
    if (FAILED(SHGetImageList(imageListType, IID_PPV_ARGS(&pImageList)))) {
        return nullptr;
    }

    HICON hIcon = nullptr;
    pImageList->GetIcon(iconIndex, ILD_TRANSPARENT, &hIcon);
    pImageList->Release();

    // Check if the high-res icon actually has content or is just a small icon
    // centered in a larger canvas with transparent padding
    if (hIcon && imageListType > SHIL_LARGE && !IconHasFullContent(hIcon, iconSize)) {
        DestroyIcon(hIcon);
        // Fall back to 32x32 — DrawIconEx will scale it up
        pImageList = nullptr;
        if (SUCCEEDED(SHGetImageList(SHIL_LARGE, IID_PPV_ARGS(&pImageList)))) {
            hIcon = nullptr;
            pImageList->GetIcon(iconIndex, ILD_TRANSPARENT, &hIcon);
            pImageList->Release();
        } else {
            hIcon = nullptr;
        }
    }

    return hIcon;
}

// Extract a high-resolution icon from the system image list using a file path.
// Returns the icon handle, or nullptr on failure.
static HICON ExtractHighResIcon(const std::wstring& path, int iconSize, DWORD fileAttributes = 0) {
    SHFILEINFOW sfi = {};
    UINT flags = SHGFI_SYSICONINDEX;
    if (fileAttributes != 0) {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }
    if (!SHGetFileInfoW(path.c_str(), fileAttributes, &sfi, sizeof(sfi), flags)) {
        return nullptr;
    }
    return ExtractFromImageList(sfi.iIcon, iconSize);
}

// Extract a high-resolution icon from the system image list using a PIDL (for special shell items).
static HICON ExtractHighResIconFromPidl(LPCITEMIDLIST pidl, int iconSize) {
    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX)) {
        return nullptr;
    }
    return ExtractFromImageList(sfi.iIcon, iconSize);
}

// ============================================================================
// Icon clearing and loading
// ============================================================================

void CorralWindow::ClearIcons() {
    for (auto& icon : icons) {
        if (icon.hIcon) {
            DestroyIcon(icon.hIcon);
            icon.hIcon = nullptr;
        }
        if (icon.hIconSmall) {
            DestroyIcon(icon.hIconSmall);
            icon.hIconSmall = nullptr;
        }
    }
    icons.clear();
    selectedIcon = -1;
}

void CorralWindow::LoadFileDetails(CorralIcon& icon) {
    // Get file type description
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(icon.fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_TYPENAME)) {
        icon.fileType = sfi.szTypeName;
    }

    // Get file size and modified time
    WIN32_FILE_ATTRIBUTE_DATA fileData = {};
    if (GetFileAttributesExW(icon.fullPath.c_str(), GetFileExInfoStandard, &fileData)) {
        icon.fileSize = ((ULONGLONG)fileData.nFileSizeHigh << 32) | fileData.nFileSizeLow;
        icon.modifiedTime = fileData.ftLastWriteTime;
    }

    // Get sync status
    icon.syncStatus = GetSyncStatus(icon.fullPath);
}

SyncStatus CorralWindow::GetSyncStatus(const std::wstring& path) {
    // Detect cloud files by their attributes and reparse tags, not by path names.
    // This works for OneDrive, Dropbox, Google Drive, iCloud, and other cloud providers.

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return SyncStatus::None;
    }

    // Check Windows cloud file attributes (most reliable method)
    // These attributes are set by cloud sync providers like OneDrive

    // Cloud-only files: not locally available, will download on access
    if (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS ||
        attrs & FILE_ATTRIBUTE_RECALL_ON_OPEN ||
        attrs & FILE_ATTRIBUTE_UNPINNED) {
        return SyncStatus::CloudOnly;
    }

    // Pinned files: "Always keep on this device" - fully synced locally
    if (attrs & FILE_ATTRIBUTE_PINNED) {
        return SyncStatus::Synced;
    }

    // Check for cloud reparse point (OneDrive uses 0x9000xxxx tags)
    // Files synced by OneDrive have reparse points even when locally available
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(path.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            /// OneDrive cloud tags: 0x9000001A through 0x9000F01A
            /// Check if high word matches 0x9000 (cloud files filter)
            DWORD tag = findData.dwReserved0;
            if ((tag & ONEDRIVE_CLOUD_TAG_MASK) == ONEDRIVE_CLOUD_TAG_MASK) {
                return SyncStatus::Synced;
            }
        }
    }

    // Not a cloud-managed file - show nothing
    return SyncStatus::None;
}

bool CorralWindow::IsSpecialIconEntry(const std::string& fileName) {
    return fileName.size() > 6 && fileName.substr(0, 6) == "shell:";
}

std::wstring CorralWindow::GetSpecialIconClsid(const std::string& fileName) {
    // "shell:{CLSID}" -> L"{CLSID}"
    std::string clsid = fileName.substr(6);
    return std::wstring(clsid.begin(), clsid.end());
}

bool CorralWindow::LoadSpecialIcon(CorralIcon& ci, const std::string& fileName, UINT iconFlag, bool isDetailsView) {
    ci.isSpecialIcon = true;
    ci.fileName = fileName;
    ci.clsid = GetSpecialIconClsid(fileName);

    // Resolve CLSID to display name via shell namespace
    ci.displayName = DesktopIcons::GetSpecialIconDisplayName(ci.clsid);
    if (ci.displayName.empty()) return false;

    ci.wFileName = ci.displayName;

    // Build parsing name "::{CLSID}" and get PIDL for icon extraction
    std::wstring parseName = L"::" + ci.clsid;
    LPITEMIDLIST pidl = nullptr;
    if (FAILED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)) || !pidl) {
        return false;
    }

    // Load icon via system image list for proper resolution
    ci.hIcon = ExtractHighResIconFromPidl(pidl, iconSize);
    if (!ci.hIcon) {
        // Fallback to SHGetFileInfo
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ICON | iconFlag)) {
            ci.hIcon = sfi.hIcon;
        }
    }

    // Load small icon for details view
    if (isDetailsView || iconSize > 16) {
        SHFILEINFOW sfiSmall = {};
        if (SHGetFileInfoW((LPCWSTR)pidl, 0, &sfiSmall, sizeof(sfiSmall),
                           SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON)) {
            ci.hIconSmall = sfiSmall.hIcon;
        }
    }

    // Load type name for details view
    if (isDetailsView) {
        SHFILEINFOW sfiType = {};
        if (SHGetFileInfoW((LPCWSTR)pidl, 0, &sfiType, sizeof(sfiType), SHGFI_PIDL | SHGFI_TYPENAME)) {
            ci.fileType = sfiType.szTypeName;
        }
    }

    CoTaskMemFree(pidl);
    return ci.hIcon != nullptr;
}

void CorralWindow::LoadIconImages() {
    ClearIcons();

    std::wstring desktopPath = GetDesktopPath();
    std::wstring publicDesktopPath = GetPublicDesktopPath();

    // Determine which icon flag to use based on view mode
    iconSize = GetIconSizeForViewMode();
    UINT iconFlag = (iconSize <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    bool isDetailsView = (GetActiveTab().GetViewMode() == ViewMode::Details);

    for (const auto& fileName : GetActiveTab().Files) {
        CorralIcon ci;

        // Check if this is a special shell icon (e.g. "shell:{645FF040-...}")
        if (IsSpecialIconEntry(fileName)) {
            if (LoadSpecialIcon(ci, fileName, iconFlag, isDetailsView)) {
                icons.push_back(std::move(ci));
            }
            continue;
        }

        ci.fileName = fileName;
        ci.wFileName = Utf8ToWide(fileName);

        // Create display name (hide .lnk extension like Windows does)
        ci.displayName = ci.wFileName;
        if (ci.displayName.length() > 4) {
            std::wstring ext = ci.displayName.substr(ci.displayName.length() - 4);
            // Case-insensitive comparison for .lnk
            if (ext == L".lnk" || ext == L".LNK" || ext == L".Lnk") {
                ci.displayName = ci.displayName.substr(0, ci.displayName.length() - 4);
            }
        }

        // Try user desktop first, then public desktop
        std::wstring userPath = desktopPath + L"\\" + ci.wFileName;
        std::wstring pubPath = publicDesktopPath + L"\\" + ci.wFileName;

        if (GetFileAttributesW(userPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ci.fullPath = userPath;
        }
        else if (GetFileAttributesW(pubPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ci.fullPath = pubPath;
        }
        else {
            ci.fullPath = userPath;
        }

        // Load the shell icon at proper resolution
        ci.hIcon = ExtractHighResIcon(ci.fullPath, iconSize);
        if (!ci.hIcon) {
            // Fallback: try SHGetFileInfo
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi),
                SHGFI_ICON | iconFlag)) {
                ci.hIcon = sfi.hIcon;
            } else {
                // Fallback: try by file extension
                DWORD fileAttribs = GetFileAttributesW(ci.fullPath.c_str());
                if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
                    fileAttribs = FILE_ATTRIBUTE_NORMAL;
                }
                ci.hIcon = ExtractHighResIcon(ci.fullPath, iconSize, fileAttribs);
                if (!ci.hIcon) {
                    if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                        SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                        ci.hIcon = sfi.hIcon;
                    }
                }
            }
        }

        // Always load small icon for details view
        if (isDetailsView || iconSize > 16) {
            SHFILEINFOW sfiSmall = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfiSmall, sizeof(sfiSmall),
                SHGFI_ICON | SHGFI_SMALLICON)) {
                ci.hIconSmall = sfiSmall.hIcon;
            } else {
                // Fallback for small icon as well
                DWORD fileAttribs = GetFileAttributesW(ci.fullPath.c_str());
                if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
                    fileAttribs = FILE_ATTRIBUTE_NORMAL;
                }
                if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfiSmall, sizeof(sfiSmall),
                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    ci.hIconSmall = sfiSmall.hIcon;
                }
            }
        }

        // Load file details for details view
        if (isDetailsView) {
            LoadFileDetails(ci);
        }

        icons.push_back(std::move(ci));
    }
}

void CorralWindow::LoadVirtualFolderIcons() {
    ClearIcons();

    if (GetActiveTab().VirtualFolderPath.empty()) return;

    std::wstring folderPath = Utf8ToWide(GetActiveTab().VirtualFolderPath);

    // Enumerate folder contents
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = folderPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    iconSize = GetIconSizeForViewMode();
    UINT iconFlag = (iconSize <= 16) ? SHGFI_SMALLICON : SHGFI_LARGEICON;
    bool isDetailsView = (GetActiveTab().GetViewMode() == ViewMode::Details);

    do {
        // Skip . and ..
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        // Skip hidden files
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            continue;
        }

        CorralIcon ci;
        ci.wFileName = findData.cFileName;
        ci.fileName = WideToUtf8(ci.wFileName);
        ci.displayName = ci.wFileName;
        ci.fullPath = folderPath + L"\\" + ci.wFileName;

        // Hide .lnk extension like Windows does
        if (ci.displayName.length() > 4) {
            std::wstring ext = ci.displayName.substr(ci.displayName.length() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".lnk") {
                ci.displayName = ci.displayName.substr(0, ci.displayName.length() - 4);
            }
        }

        // Load shell icon at proper resolution
        ci.hIcon = ExtractHighResIcon(ci.fullPath, iconSize);
        if (!ci.hIcon) {
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | iconFlag)) {
                ci.hIcon = sfi.hIcon;
            } else {
                DWORD fileAttribs = findData.dwFileAttributes;
                ci.hIcon = ExtractHighResIcon(ci.fullPath, iconSize, fileAttribs);
                if (!ci.hIcon) {
                    if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                        SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                        ci.hIcon = sfi.hIcon;
                    }
                }
            }
        }

        // Load small icon for details view
        if (isDetailsView || iconSize > 16) {
            SHFILEINFOW sfiSmall = {};
            if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfiSmall, sizeof(sfiSmall),
                              SHGFI_ICON | SHGFI_SMALLICON)) {
                ci.hIconSmall = sfiSmall.hIcon;
            } else {
                // Fallback for small icon as well
                DWORD fileAttribs = findData.dwFileAttributes;
                if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfiSmall, sizeof(sfiSmall),
                    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    ci.hIconSmall = sfiSmall.hIcon;
                }
            }
        }

        if (isDetailsView) {
            LoadFileDetails(ci);
        }

        icons.push_back(std::move(ci));
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

// ============================================================================
// Folder watching for virtual corrals
// ============================================================================

void CorralWindow::InitializeFolderWatcher() {
    if (!GetActiveTab().IsVirtual || GetActiveTab().VirtualFolderPath.empty()) return;

    folderWatcher = std::make_unique<FolderWatcher>();
    std::wstring folderPath = Utf8ToWide(GetActiveTab().VirtualFolderPath);
    folderWatcher->SetPath(folderPath);
    folderWatcher->SetChangeCallback([this]() {
        // Post message to window to handle on main thread
        if (hwnd) {
            PostMessageW(hwnd, WM_FOLDER_CHANGED, 0, 0);
        }
    });
    folderWatcher->Start();
}

void CorralWindow::OnFolderContentsChanged() {
    LoadFiles();
}

// ============================================================================
// Layout calculation
// ============================================================================

void CorralWindow::CalculateIconLayout() {
    if (GetActiveTab().GetViewMode() == ViewMode::Details) {
        CalculateIconLayoutDetails();
    } else {
        CalculateIconLayoutGrid();
    }
    // Clamp scroll position after layout change
    ClampScrollPosition();
}

void CorralWindow::RecalculateLayout() {
    UpdateIconSpacingForViewMode();
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::CalculateIconLayoutGrid() {
    int x = Dpi(ICON_PADDING_LEFT);
    int y = GetIconAreaTop();

    // Use actual window dimensions, not config (which may differ when rolled up/animating)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right;

    // Reserve space for scrollbar if needed (will be recalculated)
    int rightPadding = Dpi(ICON_PADDING_LEFT);

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        int iconImgX = x + (iconSpacingX - iconSize) / 2;
        int iconImgY = y;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
        icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

        x += iconSpacingX;
        if (x + iconSpacingX > clientWidth - rightPadding) {
            x = Dpi(ICON_PADDING_LEFT);
            y += iconSpacingY;
        }
    }

    // Calculate total content height
    if (!icons.empty()) {
        int lastIconBottom = icons.back().rect.bottom;
        contentHeight = lastIconBottom + Dpi(ICON_PADDING_LEFT);
    } else {
        contentHeight = GetIconAreaTop();
    }

    // If we need a scrollbar, recalculate with scrollbar space
    if (NeedsScrollbar() && rightPadding == Dpi(ICON_PADDING_LEFT)) {
        rightPadding = Dpi(ICON_PADDING_LEFT) + Dpi(SCROLLBAR_WIDTH) + Dpi(SCROLLBAR_MARGIN) * 2;

        x = Dpi(ICON_PADDING_LEFT);
        y = GetIconAreaTop();

        for (size_t i = 0; i < icons.size(); i++) {
            auto& icon = icons[i];

            int iconImgX = x + (iconSpacingX - iconSize) / 2;
            int iconImgY = y;

            icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
            icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

            x += iconSpacingX;
            if (x + iconSpacingX > clientWidth - rightPadding) {
                x = Dpi(ICON_PADDING_LEFT);
                y += iconSpacingY;
            }
        }

        // Recalculate content height
        if (!icons.empty()) {
            int lastIconBottom = icons.back().rect.bottom;
            contentHeight = lastIconBottom + Dpi(ICON_PADDING_LEFT);
        }
    }
}

void CorralWindow::CalculateIconLayoutDetails() {
    // Details view: list layout with rows
    // Each row has: [icon 16px] [name] [type] [size] [date] [sync status]
    int y = GetIconAreaTop();

    // Use actual window dimensions, not config (which may differ when rolled up/animating)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right;
    int rightPadding = Dpi(ICON_PADDING_LEFT);

    // Check if we'll need scrollbar
    int detailsRow = Dpi(DETAILS_ROW_HEIGHT);
    int detailsIcon = Dpi(ICON_SIZE_DETAILS);
    int estimatedHeight = GetIconAreaTop() + (int)icons.size() * detailsRow + Dpi(ICON_PADDING_LEFT);
    if (estimatedHeight > clientRect.bottom) {
        rightPadding = Dpi(ICON_PADDING_LEFT) + Dpi(SCROLLBAR_WIDTH) + Dpi(SCROLLBAR_MARGIN) * 2;
    }

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        // Icon is at the left of each row
        int iconImgX = Dpi(ICON_PADDING_LEFT) + Dpi(2);
        int iconImgY = y + (detailsRow - detailsIcon) / 2;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + detailsIcon, iconImgY + detailsIcon };
        // Full row rect for selection and hit testing
        icon.rect = { Dpi(ICON_PADDING_LEFT), y, clientWidth - rightPadding, y + detailsRow };

        y += detailsRow;
    }

    // Calculate total content height
    if (!icons.empty()) {
        contentHeight = y + Dpi(ICON_PADDING_LEFT);
    } else {
        contentHeight = GetIconAreaTop();
    }
}

// ============================================================================
// Hit testing
// ============================================================================

int CorralWindow::HitTestIcon(int x, int y) {
    // Adjust for scroll offset
    int adjustedY = y + scrollPosition;
    POINT pt = { x, adjustedY };

    for (int i = 0; i < (int)icons.size(); i++) {
        if (PtInRect(&icons[i].rect, pt)) {
            return i;
        }
    }
    return -1;
}

RECT CorralWindow::GetIconLabelRect(int iconIndex) const {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) {
        return { 0, 0, 0, 0 };
    }

    const CorralIcon& icon = icons[iconIndex];

    // For details view, label is in the name column
    if (GetActiveTab().GetViewMode() == ViewMode::Details) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int clientWidth = rect.right - rect.left;
        int rightPadding = Dpi(ICON_PADDING_LEFT);
        if (NeedsScrollbar()) {
            rightPadding = Dpi(ICON_PADDING_LEFT) + Dpi(SCROLLBAR_WIDTH) + Dpi(SCROLLBAR_MARGIN) * 2;
        }
        int contentWidth = clientWidth - Dpi(ICON_PADDING_LEFT) - rightPadding;

        int nameCol = icon.iconRect.left + Dpi(ICON_SIZE_DETAILS) + Dpi(4);
        int typeCol = nameCol + (int)(contentWidth * 0.40);

        return { nameCol, icon.rect.top, typeCol - Dpi(4), icon.rect.bottom };
    }

    // For icon views, label is below the icon
    int labelTop = icon.iconRect.bottom + Dpi(2);
    return { icon.rect.left, labelTop, icon.rect.right, icon.rect.bottom };
}

bool CorralWindow::HitTestIconLabel(int x, int y, int iconIndex) const {
    if (iconIndex < 0 || iconIndex >= (int)icons.size()) {
        return false;
    }

    // Get label rect in content coordinates and convert to screen coordinates
    RECT labelRect = GetIconLabelRect(iconIndex);
    labelRect.top -= scrollPosition;
    labelRect.bottom -= scrollPosition;

    POINT pt = { x, y };
    return PtInRect(&labelRect, pt) != FALSE;
}

// ============================================================================
// Per-icon screen position calculation for desktop icon sync
// ============================================================================

std::map<std::wstring, POINT2D> CorralWindow::GetIconScreenPositions() const {
    std::map<std::wstring, POINT2D> result;
    if (icons.empty() || !hwnd) return result;

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) return result;

    // Get the desktop ListView for coordinate conversion
    HWND hListView = DesktopIcons::GetDesktopListView();

    int visibleTop = GetIconAreaTop();
    int visibleBottom = GetVisibleHeight();

    for (const auto& icon : icons) {
        // Calculate screen-space position of this icon's center (accounting for scroll)
        int iconCenterX = (icon.iconRect.left + icon.iconRect.right) / 2;
        int iconCenterY = (icon.iconRect.top + icon.iconRect.bottom) / 2 - scrollPosition;

        // Check if icon is within the visible viewport
        bool inViewport = (icon.rect.bottom - scrollPosition > visibleTop) &&
                          (icon.rect.top - scrollPosition < visibleBottom);

        POINT screenPt;
        if (inViewport) {
            // Icon is visible: position at its actual screen location
            screenPt = { windowRect.left + iconCenterX, windowRect.top + iconCenterY };
        } else {
            // Icon is scrolled out: position just outside the corral edge
            screenPt.x = windowRect.left + iconCenterX;
            if (icon.rect.top - scrollPosition >= visibleBottom) {
                // Below viewport: place just below corral bottom
                screenPt.y = windowRect.bottom + 10;
            } else {
                // Above viewport: place just above corral top
                screenPt.y = windowRect.top - 10;
            }
        }

        // Convert screen coords to ListView client coords
        if (hListView) {
            ScreenToClient(hListView, &screenPt);
        }

        // Use display name as key (matches what HookBridge uses)
        if (!icon.displayName.empty()) {
            result[icon.displayName] = { screenPt.x, screenPt.y };
        }
    }

    return result;
}

// ============================================================================
// View mode support
// ============================================================================

int CorralWindow::GetIconSizeForViewMode() const {
    int desktopSize = GetDesktopIconSize();  // logical pixels from registry
    int size;

    switch (GetActiveTab().GetViewMode()) {
    case ViewMode::SmallIcons:
        size = 32;
        break;
    case ViewMode::MediumIcons:
        // Medium = desktop icon size, but at least 48 so it differs from small
        size = (desktopSize <= 32) ? 48 : desktopSize;
        break;
    case ViewMode::LargeIcons: {
        // Large = 2x desktop icon size, minimum 96
        int largeSize = desktopSize * 2;
        size = (largeSize < 96) ? 96 : largeSize;
        break;
    }
    case ViewMode::Details:
        size = ICON_SIZE_DETAILS;
        break;
    default:
        size = desktopSize;
        break;
    }

    return Dpi(size);
}

void CorralWindow::UpdateIconSpacingForViewMode() {
    if (GetActiveTab().GetViewMode() == ViewMode::Details) {
        iconSpacingX = Dpi(72);
        iconSpacingY = Dpi(DETAILS_ROW_HEIGHT);
        return;
    }

    // Use desktop-like grid spacing (queries desktop ListView)
    GetDesktopIconSpacing(iconSpacingX, iconSpacingY);

    // Apply user's per-corral spacing multiplier
    iconSpacingX = iconSpacingX * config.IconSpacingXPercent / 100;
    iconSpacingY = iconSpacingY * config.IconSpacingYPercent / 100;

    // Ensure icons don't overlap (minimum = icon size + small label space)
    if (iconSpacingX < iconSize + Dpi(16)) iconSpacingX = iconSize + Dpi(16);
    if (iconSpacingY < iconSize + Dpi(20)) iconSpacingY = iconSize + Dpi(20);
}
