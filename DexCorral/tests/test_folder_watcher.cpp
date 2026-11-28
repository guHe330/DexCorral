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
    std::atomic<int> endings{0};

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

    /// Touches the file until a change is reported. Not once after a fixed
    /// pause: a change made before the watch thread has opened the folder is
    /// not reported, and how long that takes depends on the machine.
    bool TouchUntilChange(DWORD ms) {
        for (DWORD waited = 0; waited < ms; waited += 50) {
            Touch();
            Sleep(50);
            if (changes > 0)
                return true;
        }
        return false;
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
    watcher.SetChangeCallback([this](bool) { changes++; });
    watcher.Start();

    EXPECT_TRUE(TouchUntilChange(5000));
}

TEST_F(WatchedFolder, IsQuietAfterStop) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this](bool) { changes++; });
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
    watcher.SetChangeCallback([this](bool) { changes++; });
    watcher.Start();
    watcher.Stop();
    watcher.Stop(); // Twice is harmless
    watcher.Start();

    EXPECT_TRUE(TouchUntilChange(5000));
}

TEST_F(WatchedFolder, AFolderThatDoesNotExistIsNotAnError) {
    FolderWatcher watcher;
    watcher.SetPath(dir + L"\\missing");
    watcher.SetChangeCallback([this](bool) { changes++; });
    watcher.Start();
    Sleep(100);
    watcher.Stop();
    EXPECT_EQ(changes, 0);
}

TEST_F(WatchedFolder, SaysOnceWhenTheFolderIsDeleted) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this](bool ended) { ended ? endings++ : changes++; });
    watcher.Start();
    ASSERT_TRUE(TouchUntilChange(5000));

    DeleteFileW((dir + L"\\a.txt").c_str());
    // A scanner may still hold the file for a moment, which keeps the folder from going
    bool removed = false;
    for (int tries = 0; tries < 100 && !(removed = RemoveDirectoryW(dir.c_str())); tries++)
        Sleep(20);
    ASSERT_TRUE(removed);
    for (DWORD waited = 0; waited < 5000 && endings == 0; waited += 20)
        Sleep(20);
    EXPECT_EQ(endings, 1);

    // Made again under the same name: a new Start() watches it
    ASSERT_TRUE(CreateDirectoryW(dir.c_str(), nullptr));
    watcher.Stop();
    changes = 0;
    watcher.Start();
    EXPECT_TRUE(TouchUntilChange(5000));
    EXPECT_EQ(endings, 1);
}

TEST_F(WatchedFolder, StopIsNotAnEnding) {
    FolderWatcher watcher;
    watcher.SetPath(dir);
    watcher.SetChangeCallback([this](bool ended) { ended ? endings++ : changes++; });
    watcher.Start();
    ASSERT_TRUE(TouchUntilChange(5000));
    watcher.Stop();
    Sleep(100);
    EXPECT_EQ(endings, 0);
}
