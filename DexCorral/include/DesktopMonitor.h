#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class DesktopMonitor {
public:
    using FileAddedCallback = std::function<void(const std::wstring& fileName)>;
    using FileRenamedCallback = std::function<void(const std::wstring& oldName, const std::wstring& newName)>;
    using FileDeletedCallback = std::function<void(const std::wstring& fileName)>;

    DesktopMonitor();
    ~DesktopMonitor();

    void SetFileAddedCallback(FileAddedCallback callback) { fileAddedCallback = callback; }
    void SetFileRenamedCallback(FileRenamedCallback callback) { fileRenamedCallback = callback; }
    void SetFileDeletedCallback(FileDeletedCallback callback) { fileDeletedCallback = callback; }

    void Start();
    void Stop();

private:
    void MonitorThread(const std::wstring& path);
    void ProcessNotification(BYTE* buffer, DWORD bytesReturned);

    static std::wstring GetDesktopPath();
    static std::wstring GetPublicDesktopPath();

    FileAddedCallback fileAddedCallback;
    FileRenamedCallback fileRenamedCallback;
    FileDeletedCallback fileDeletedCallback;

    std::atomic<bool> running;
    std::thread userDesktopThread;
    std::thread publicDesktopThread;
    HANDLE userStopEvent = nullptr;
    HANDLE publicStopEvent = nullptr;

    // For tracking renames (old name is stored until new name arrives)
    std::wstring pendingOldName;
};
