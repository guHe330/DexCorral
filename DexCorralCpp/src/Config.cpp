#include "Config.h"
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <filesystem>

std::string Config::GetConfigPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring wpath(path);
        wpath += L"\\DexCorral";

        // Create directory if it doesn't exist
        CreateDirectoryW(wpath.c_str(), NULL);

        wpath += L"\\config.json";

        // Convert to UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    return "config.json";
}

AppConfig Config::Load() {
    std::string path = GetConfigPath();
    std::ifstream file(path);

    if (!file.is_open()) {
        return AppConfig();
    }

    try {
        nlohmann::json j;
        file >> j;
        return j.get<AppConfig>();
    }
    catch (...) {
        return AppConfig();
    }
}

void Config::Save(const AppConfig& config) {
    std::string path = GetConfigPath();
    std::ofstream file(path);

    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json j = config;
        file << j.dump(4);
    }
    catch (...) {
        // Error saving config
    }
}
