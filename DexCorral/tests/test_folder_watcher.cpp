/**
 * test_folder_watcher.cpp - FolderWatcher against a real folder under %TEMP%.
 */

#include <gtest/gtest.h>
#include "FolderWatcher.h"
#include <Windows.h>
#include <atomic>
#include <fstream>

// The watcher logs through the DLL's debug log; the tests have none
void DllLog(const wchar_t *, ...) {}

namespace {

class WatchedFolder : public ::testing::Test {
protected:
    std::wstring dir;
    std::atomic<int> changes{0};

    void SetUp() override {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        dir = std::wstring(temp) + L"DexCorralWatch_" + std::to_wstring(GetCurrentProcessId()) +
              L"_" + std::to_wstring(GetTickCount64());
        ASSERT_TRUE(CreateDirectoryW(dir.c_str(), nullptr));
    }

    void TearDown() override {
        DeleteFileW((dir + L"\\a.txt").c_str());
        RemoveDirectoryW(dir.c_str());
    }

    void Touch() {
        std::ofstream f((dir + L"\\a.txt").c_str(), std::ios::trunc);
        f << GetTickCount64();
    }

    bool WaitForChange(int moreThan, DWORD ms) {
        for (DWORD waited = 0; waited < ms; waited += 20) {
            if (changes > moreThan)
                return true;
            Sleep(20);
        }
        return changes > moreThan;
    }
};

} // namespace

TEST_F(WatchedFolder, ReportsAChange) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this] { changes++; });
    watcher.Start();
    Sleep(200); // Let the watch thread open the folder

    Touch();
    EXPECT_TRUE(WaitForChange(0, 3000));
}

TEST_F(WatchedFolder, IsQuietAfterStop) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this] { changes++; });
    watcher.Start();
    Sleep(200);
    watcher.Stop();

    const int before = changes;
    Touch();
    EXPECT_FALSE(WaitForChange(before, 500));
}

TEST_F(WatchedFolder, StopAndRestartKeepWorking) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this] { changes++; });
    watcher.Start();
    watcher.Stop();
    watcher.Stop(); // Twice is harmless
    watcher.Start();
    Sleep(200);

    Touch();
    EXPECT_TRUE(WaitForChange(0, 3000));
}

TEST_F(WatchedFolder, AFolderThatDoesNotExistIsNotAnError) {
    FolderWatcher watcher;
    watcher.SetPath(dir + L"\\missing");
    watcher.SetChangeCallback([this] { changes++; });
    watcher.Start();
    Sleep(100);
    watcher.Stop();
    EXPECT_EQ(changes, 0);
}
