// CorralWindowIcons.cpp - Icon loading, layout calculation, and hit testing
#include "CorralWindow.h"
#include "FolderWatcher.h"
#include <shellapi.h>
#include <ShlObj.h>
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
            // OneDrive cloud tags: 0x9000001A through 0x9000F01A
            // Check if high word matches 0x9000 (cloud files filter)
            DWORD tag = findData.dwReserved0;
            if ((tag & 0xFFFF0000) == 0x90000000) {
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

    // Load icon via PIDL
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW((LPCWSTR)pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ICON | iconFlag)) {
        ci.hIcon = sfi.hIcon;
    }

    // Load small icon
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
        ci.wFileName = std::wstring(fileName.begin(), fileName.end());

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

        // Load the shell icon
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi),
            SHGFI_ICON | iconFlag)) {
            ci.hIcon = sfi.hIcon;
        } else {
            // Fallback: Try to get icon by file extension instead of actual file
            // This helps with corrupted icon cache or permission issues
            DWORD fileAttribs = GetFileAttributesW(ci.fullPath.c_str());
            if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
                fileAttribs = FILE_ATTRIBUTE_NORMAL;
            }
            if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                ci.hIcon = sfi.hIcon;
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

        // Load shell icon
        SHFILEINFOW sfi = {};
        if (SHGetFileInfoW(ci.fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | iconFlag)) {
            ci.hIcon = sfi.hIcon;
        } else {
            // Fallback: Try to get icon by file extension instead of actual file
            // This helps with corrupted icon cache or permission issues
            DWORD fileAttribs = findData.dwFileAttributes;
            if (SHGetFileInfoW(ci.fullPath.c_str(), fileAttribs, &sfi, sizeof(sfi),
                SHGFI_ICON | iconFlag | SHGFI_USEFILEATTRIBUTES)) {
                ci.hIcon = sfi.hIcon;
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

void CorralWindow::CalculateIconLayoutGrid() {
    int x = ICON_PADDING_LEFT;
    int y = ICON_AREA_TOP;

    // Use actual window dimensions, not config (which may differ when rolled up/animating)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right;

    // Reserve space for scrollbar if needed (will be recalculated)
    int rightPadding = ICON_PADDING_LEFT;

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        int iconImgX = x + (iconSpacingX - iconSize) / 2;
        int iconImgY = y;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
        icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

        x += iconSpacingX;
        if (x + iconSpacingX > clientWidth - rightPadding) {
            x = ICON_PADDING_LEFT;
            y += iconSpacingY;
        }
    }

    // Calculate total content height
    if (!icons.empty()) {
        int lastIconBottom = icons.back().rect.bottom;
        contentHeight = lastIconBottom + ICON_PADDING_LEFT;
    } else {
        contentHeight = ICON_AREA_TOP;
    }

    // If we need a scrollbar, recalculate with scrollbar space
    if (NeedsScrollbar() && rightPadding == ICON_PADDING_LEFT) {
        rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;

        x = ICON_PADDING_LEFT;
        y = ICON_AREA_TOP;

        for (size_t i = 0; i < icons.size(); i++) {
            auto& icon = icons[i];

            int iconImgX = x + (iconSpacingX - iconSize) / 2;
            int iconImgY = y;

            icon.iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
            icon.rect = { x, y, x + iconSpacingX, y + iconSpacingY };

            x += iconSpacingX;
            if (x + iconSpacingX > clientWidth - rightPadding) {
                x = ICON_PADDING_LEFT;
                y += iconSpacingY;
            }
        }

        // Recalculate content height
        if (!icons.empty()) {
            int lastIconBottom = icons.back().rect.bottom;
            contentHeight = lastIconBottom + ICON_PADDING_LEFT;
        }
    }
}

void CorralWindow::CalculateIconLayoutDetails() {
    // Details view: list layout with rows
    // Each row has: [icon 16px] [name] [type] [size] [date] [sync status]
    int y = ICON_AREA_TOP;

    // Use actual window dimensions, not config (which may differ when rolled up/animating)
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right;
    int visibleHeight = clientRect.bottom - ICON_AREA_TOP;
    int rightPadding = ICON_PADDING_LEFT;

    // Check if we'll need scrollbar
    int estimatedHeight = ICON_AREA_TOP + (int)icons.size() * DETAILS_ROW_HEIGHT + ICON_PADDING_LEFT;
    if (estimatedHeight > visibleHeight) {
        rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;
    }

    for (size_t i = 0; i < icons.size(); i++) {
        auto& icon = icons[i];

        // Icon is at the left of each row
        int iconImgX = ICON_PADDING_LEFT + 2;
        int iconImgY = y + (DETAILS_ROW_HEIGHT - ICON_SIZE_DETAILS) / 2;

        icon.iconRect = { iconImgX, iconImgY, iconImgX + ICON_SIZE_DETAILS, iconImgY + ICON_SIZE_DETAILS };
        // Full row rect for selection and hit testing
        icon.rect = { ICON_PADDING_LEFT, y, clientWidth - rightPadding, y + DETAILS_ROW_HEIGHT };

        y += DETAILS_ROW_HEIGHT;
    }

    // Calculate total content height
    if (!icons.empty()) {
        contentHeight = y + ICON_PADDING_LEFT;
    } else {
        contentHeight = ICON_AREA_TOP;
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
        int rightPadding = ICON_PADDING_LEFT;
        if (NeedsScrollbar()) {
            rightPadding = ICON_PADDING_LEFT + SCROLLBAR_WIDTH + SCROLLBAR_MARGIN * 2;
        }
        int contentWidth = clientWidth - ICON_PADDING_LEFT - rightPadding;

        int nameCol = icon.iconRect.left + ICON_SIZE_DETAILS + 4;
        int typeCol = nameCol + (int)(contentWidth * 0.40);

        return { nameCol, icon.rect.top, typeCol - 4, icon.rect.bottom };
    }

    // For icon views, label is below the icon
    int labelTop = icon.iconRect.bottom + 2;
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
// View mode support
// ============================================================================

int CorralWindow::GetIconSizeForViewMode() const {
    switch (GetActiveTab().GetViewMode()) {
    case ViewMode::SmallIcons: return ICON_SIZE_SMALL;
    case ViewMode::MediumIcons: return ICON_SIZE_MEDIUM;
    case ViewMode::LargeIcons: return ICON_SIZE_LARGE;
    case ViewMode::Details: return ICON_SIZE_DETAILS;
    default: return ICON_SIZE_SMALL;
    }
}

void CorralWindow::UpdateIconSpacingForViewMode() {
    switch (GetActiveTab().GetViewMode()) {
    case ViewMode::SmallIcons:
        iconSpacingX = 72;
        iconSpacingY = 68;
        break;
    case ViewMode::MediumIcons:
        iconSpacingX = 80;
        iconSpacingY = 80;
        break;
    case ViewMode::LargeIcons:
        // Larger spacing for large icons to fit longer labels
        iconSpacingX = 100;
        iconSpacingY = 100;
        break;
    case ViewMode::Details:
        // Not used for details view, but set reasonable defaults
        iconSpacingX = 72;
        iconSpacingY = DETAILS_ROW_HEIGHT;
        break;
    default:
        iconSpacingX = 72;
        iconSpacingY = 68;
        break;
    }
}
