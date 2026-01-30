#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <string>
#include <memory>

class WallpaperManager {
public:
    WallpaperManager();
    ~WallpaperManager();

    bool LoadWallpaper();
    void RenderWallpaperRegion(HDC hdc, double x, double y, double width, double height);
    std::wstring GetWallpaperPath();

private:
    std::unique_ptr<Gdiplus::Image> wallpaperImage;
    std::wstring currentWallpaperPath;
    ULONG_PTR gdiplusToken;
};
