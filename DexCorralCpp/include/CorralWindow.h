#pragma once
#include <Windows.h>
#include <oleidl.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "Config.h"
#include "DesktopIcons.h"

class WallpaperManager;
class FolderWatcher;
class CorralWindow;

// OLE drop target for corral windows - accepts both regular files and special shell items
class CorralDropTarget : public IDropTarget {
public:
    CorralDropTarget(CorralWindow* owner);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

private:
    LONG refCount;
    CorralWindow* owner;
    bool hasDropData;
};

// Sync status for cloud storage providers
enum class SyncStatus {
    None = 0,       // Not a synced file or unknown
    Synced,         // Fully synced (green checkmark)
    Syncing,        // Currently syncing (blue arrows)
    Pending,        // Pending sync (blue clock)
    Error,          // Sync error (red X)
    CloudOnly       // Available online only (cloud icon)
};

struct CorralIcon {
    std::string fileName;       // UTF-8 filename (for config), or "shell:{CLSID}" for special icons
    std::wstring wFileName;     // Wide filename (actual name)
    std::wstring displayName;   // Display name (without .lnk extension)
    std::wstring fullPath;      // Full path to file on desktop (empty for special icons)
    HICON hIcon = nullptr;      // Shell icon handle
    HICON hIconSmall = nullptr; // Small icon for details view (16px)
    RECT rect = {};             // Bounding rect in client coords (icon + label)
    RECT iconRect = {};         // Just the icon image rect

    // Special shell icon support (Recycle Bin, This PC, etc.)
    bool isSpecialIcon = false;  // True if this is a virtual shell item
    std::wstring clsid;          // CLSID string for special icons (without :: prefix)

    // Details view info
    std::wstring fileType;      // File type description
    ULONGLONG fileSize = 0;     // File size in bytes
    FILETIME modifiedTime = {}; // Last modified time
    SyncStatus syncStatus = SyncStatus::None;
};

class CorralWindow {
public:
    CorralWindow(const CorralWindowConfig& config, WallpaperManager* wallpaperMgr);
    ~CorralWindow();

    void Show();
    void Hide();
    void SendToBottom();
    void UpdateWallpaperBackground();
    void LoadFiles();
    void SyncConfigFromWindow();  // Update config from current window state
    void AddFile(const std::string& fileName);  // Add a file to this corral (active tab)

    // Special icon helpers (public for use by App)
    static bool IsSpecialIconEntry(const std::string& fileName);
    static std::wstring GetSpecialIconClsid(const std::string& fileName);

    HWND GetHWND() const { return hwnd; }
    CorralWindowConfig& GetConfig() { return config; }
    const CorralWindowConfig& GetConfig() const { return config; }
    int GetTitleBarHeight() const { return Dpi(config.TitleBarHeight); }
    int GetIconAreaTop() const { return Dpi(config.TitleBarHeight + 4); }
    void RecalculateLayout();  // Public wrapper for icon layout recalculation
    void SetCurrentOpacity(int opacity) { currentOpacity = (opacity < 5) ? 5 : opacity; }

    // Tab helpers
    CorralTabConfig& GetActiveTab();
    const CorralTabConfig& GetActiveTab() const;
    void SetActiveTab(int index);
    void AddTab(const CorralTabConfig& tab);
    void DetachTab(int tabIndex);
    void MergeWith(CorralWindow* other);

    // Virtual corral support
    void ChangeFolderPath();  // Open folder browser to change virtual folder path

private:
    // Virtual corral support
    void LoadVirtualFolderIcons();  // Load icons from virtual folder path
    void InitializeFolderWatcher();  // Set up folder change monitoring
    void OnFolderContentsChanged();  // Called when virtual folder contents change

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void UpdateLayeredContent();  // True per-pixel transparency rendering
    void OnSize();
    void OnMove();
    void OnLeftButtonDown(int x, int y);
    void OnLeftButtonUp(int x, int y);  // Added for drag-end detection
    void OnLeftButtonDblClick(int x, int y);
    void OnRightButtonDown(int x, int y);
    void OnDrop(IDataObject* pDataObj);  // OLE drop handler (replaces OnDropFiles)

    friend class CorralDropTarget;

    void ShowContextMenu(int x, int y);
    void ShowRenameDialog();
    void ShowAppearanceDialog();
    void DeleteCorral();
    void ToggleRollUp();
    void ToggleCatchAll();
    void SetViewMode(ViewMode mode);
    void ShowViewMenu(int screenX, int screenY);

    // Hover-expand for rolled-up corrals
    void StartHoverExpand();
    void StartHoverCollapse();
    void OnAnimationTimer();
    void OnHoverCheckTimer();

    // Opacity hover animation
    void StartOpacityAnimation(int target);
    void OnOpacityAnimationTimer();

    // Snap support
    void ApplySnap(int& newLeft, int& newTop, int width, int height);
    void ApplyResizeSnap(int& newLeft, int& newTop, int& newWidth, int& newHeight, int resizeMode);

    // Icon reordering
    void OnIconDrag(int x, int y);
    void OnIconDragEnd();

    // Icon rename
    RECT GetIconLabelRect(int iconIndex) const;
    bool HitTestIconLabel(int x, int y, int iconIndex) const;
    void StartIconRename(int iconIndex);
    void EndIconRename(bool save);
    void OnEditCommand(HWND hEdit, UINT command);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    // Scrollbar support
    bool NeedsScrollbar() const;
    int GetContentHeight() const;
    int GetVisibleHeight() const;
    RECT GetScrollbarTrackRect() const;
    RECT GetScrollbarThumbRect() const;
    bool HitTestScrollbar(int x, int y) const;
    bool HitTestScrollbarThumb(int x, int y) const;
    void OnMouseWheel(int delta);
    void StartScrollbarDrag(int y);
    void DoScrollbarDrag(int y);
    void EndScrollbarDrag();
    void ClampScrollPosition();

    void LoadIconImages();
    void ClearIcons();
    bool LoadSpecialIcon(CorralIcon& ci, const std::string& fileName, UINT iconFlag, bool isDetailsView);
    void CalculateIconLayout();
    void CalculateIconLayoutGrid();    // Grid layout for icon views
    void CalculateIconLayoutDetails(); // List layout for details view
    int HitTestIcon(int x, int y);
    int HitTestTab(int x, int y);      // Returns tab index or -1
    RECT GetTabRect(int index) const;  // Get rect for a specific tab
    void OpenFile(int iconIndex);
    void ShowShellContextMenu(int iconIndex, int screenX, int screenY);
    void LoadFileDetails(CorralIcon& icon);  // Load file info for details view
    SyncStatus GetSyncStatus(const std::wstring& path);  // Detect sync provider status
    int GetIconSizeForViewMode() const;  // Get icon size based on current view mode
    void UpdateIconSpacingForViewMode();  // Update spacing based on current view mode

    // Resize support
    int HitTestResize(int x, int y);  // Returns HTRIGHT, HTBOTTOM, HTBOTTOMRIGHT, or 0
    void StartResize(int hitTest, int x, int y);
    void DoResize(int x, int y);
    void EndResize();

    static std::wstring GetDesktopPath();
    static std::wstring GetPublicDesktopPath();
    static int GetDesktopIconSize();  // Read from registry (logical pixels)
    void GetDesktopIconSpacing(int& spacingX, int& spacingY);  // Query desktop ListView

    // DPI scaling - applies monitor DPI factor to logical pixel values.
    // Controlled by s_enableDpiScaling flag for easy toggling.
    int Dpi(int logicalPixels) const;
    static bool s_enableDpiScaling;

    // String conversion utilities (used across split implementation files)
    static std::string WideToUtf8(const std::wstring& wide);
    static std::wstring Utf8ToWide(const std::string& utf8);

    HWND hwnd;
    CorralWindowConfig config;
    WallpaperManager* wallpaperManager;
    bool isDragging;
    POINT dragStart;
    RECT dragStartRect;

    // Resize state
    bool isResizing = false;
    int resizeMode = 0;  // HTRIGHT, HTBOTTOM, HTBOTTOMRIGHT
    POINT resizeStart = {};
    RECT resizeStartRect = {};

    std::vector<CorralIcon> icons;
    int selectedIcon = -1;

    // Icon layout - dynamically calculated from view mode
    int iconSize = 32;
    int iconSpacingX = 72;
    int iconSpacingY = 68;
    static const int ICON_PADDING_LEFT = 8;
    static const int RESIZE_BORDER = 6;
    static const int SNAP_DISTANCE = 15;  // Pixels to trigger snap
    static const int SNAP_GAP = 10;       // Gap between snapped corrals

    // Icon sizes for different view modes
    static const int ICON_SIZE_SMALL = 32;
    static const int ICON_SIZE_MEDIUM = 48;
    static const int ICON_SIZE_LARGE = 64;
    static const int ICON_SIZE_DETAILS = 16;  // Small icon for details/list view
    static const int DETAILS_ROW_HEIGHT = 20; // Height of each row in details view

    // Roll-up state
    double savedHeight = 200;  // Height before roll-up

    // Hover-expand state (for rolled-up corrals)
    bool isHoverExpanded = false;      // Currently expanded due to hover
    bool isAnimating = false;          // Animation in progress
    bool expandUpward = false;         // Expand upward (near bottom of screen)
    int animationStartHeight = 0;      // Height at animation start
    int animationTargetHeight = 0;     // Target height for animation
    int animationStartTop = 0;         // Top position at animation start
    int animationTargetTop = 0;        // Target top for upward expansion
    DWORD animationStartTime = 0;      // GetTickCount at animation start
    static const int ANIMATION_DURATION = 300;  // ms
    static const UINT_PTR ANIMATION_TIMER_ID = 1;
    static const UINT_PTR HOVER_CHECK_TIMER_ID = 2;
    bool mouseInsideWindow = false;    // Track mouse presence

    // Opacity hover animation state
    static const UINT_PTR OPACITY_TIMER_ID = 3;
    static const int OPACITY_ANIMATION_DURATION = 200;  // ms
    bool isOpacityAnimating = false;
    DWORD opacityAnimationStartTime = 0;
    int opacityStart = 255;
    int opacityTarget = 255;
    int currentOpacity = 255;  // Current animated opacity (used by render)

    // Icon dragging for reordering
    bool isDraggingIcon = false;
    int draggedIconIndex = -1;
    POINT iconDragStart = {};
    int dropTargetIndex = -1;
    bool iconDragOutside = false;  // True when icon is dragged outside corral bounds
    static const int DRAG_THRESHOLD = 5;  // Pixels to move before starting drag

    // Scrollbar state
    int scrollPosition = 0;        // Current scroll offset in pixels
    int contentHeight = 0;         // Total height of all icons
    bool isDraggingScrollbar = false;
    int scrollbarDragStartY = 0;   // Mouse Y at drag start
    int scrollbarDragStartPos = 0; // Scroll position at drag start
    static const int SCROLLBAR_WIDTH = 10;
    static const int SCROLLBAR_MARGIN = 2;
    static const int SCROLLBAR_MIN_THUMB = 30;

    // Icon rename state
    bool isRenamingIcon = false;
    int renamingIconIndex = -1;
    HWND hEditControl = nullptr;
    std::wstring originalName;

    // Virtual corral support
    std::unique_ptr<FolderWatcher> folderWatcher;
    static const UINT WM_FOLDER_CHANGED = WM_USER + 100;
    static const UINT WM_DEFERRED_LOAD = WM_USER + 101;

    // OLE drop target
    CorralDropTarget* dropTarget = nullptr;
};
