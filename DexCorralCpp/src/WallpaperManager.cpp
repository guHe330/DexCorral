#include "WallpaperManager.h"
#include <gdiplus.h>

using namespace Gdiplus;

WallpaperManager::WallpaperManager() : gdiplusToken(0) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
}

WallpaperManager::~WallpaperManager() {
    if (gdiplusToken) {
        GdiplusShutdown(gdiplusToken);
    }
}

std::wstring WallpaperManager::GetWallpaperPath() {
    wchar_t wallpaperPath[MAX_PATH];
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, wallpaperPath, 0)) {
        return std::wstring(wallpaperPath);
    }
    return L"";
}

bool WallpaperManager::LoadWallpaper() {
    std::wstring path = GetWallpaperPath();

    if (path.empty()) {
        wallpaperImage.reset();
        currentWallpaperPath.clear();
        return false;
    }

    // Check if already loaded
    if (path == currentWallpaperPath && wallpaperImage != nullptr) {
        return true;
    }

    // Load new wallpaper
    wallpaperImage = std::make_unique<Image>(path.c_str());

    if (wallpaperImage->GetLastStatus() != Ok) {
        wallpaperImage.reset();
        currentWallpaperPath.clear();
        return false;
    }

    currentWallpaperPath = path;
    return true;
}

void WallpaperManager::RenderWallpaperRegion(HDC hdc, double x, double y, double width, double height) {
    if (!wallpaperImage) {
        LoadWallpaper();
    }

    if (!wallpaperImage) {
        // Fallback: fill with black
        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
        RECT rect = { 0, 0, (LONG)width, (LONG)height };
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        return;
    }

    Graphics graphics(hdc);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Get image dimensions
    UINT imageWidth = wallpaperImage->GetWidth();
    UINT imageHeight = wallpaperImage->GetHeight();

    // Calculate source rectangle (portion of wallpaper to show)
    RectF srcRect(
        (REAL)(x / screenWidth * imageWidth),
        (REAL)(y / screenHeight * imageHeight),
        (REAL)(width / screenWidth * imageWidth),
        (REAL)(height / screenHeight * imageHeight)
    );

    // Destination rectangle (entire window)
    RectF destRect(0, 0, (REAL)width, (REAL)height);

    // Draw the wallpaper region
    graphics.DrawImage(
        wallpaperImage.get(),
        destRect,
        srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height,
        UnitPixel
    );
}
