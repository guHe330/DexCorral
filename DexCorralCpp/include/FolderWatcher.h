#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class FolderWatcher {
public:
    using ChangeCallback = std::function<void()>;

    FolderWatcher();
    ~FolderWatcher();

    void SetPath(const std::wstring& path);
    void SetChangeCallback(ChangeCallback callback) { changeCallback = callback; }

    void Start();
    void Stop();
    bool IsRunning() const { return running; }
    const std::wstring& GetPath() const { return watchPath; }

private:
    void MonitorThread();

    std::wstring watchPath;
    ChangeCallback changeCallback;

    std::atomic<bool> running;
    std::thread monitorThread;
    HANDLE stopEvent = nullptr;

    // Debouncing
    std::atomic<DWORD> lastChangeTime;
    static const DWORD DEBOUNCE_MS = 500;
};
