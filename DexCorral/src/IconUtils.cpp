#include "IconUtils.h"
#include <string.h>  // _wcsicmp

namespace IconUtils {

bool IsSpecialIconEntry(const std::string& fileName) {
    return fileName.size() > 6 && fileName.substr(0, 6) == "shell:";
}

std::wstring GetSpecialIconClsid(const std::string& fileName) {
    // "shell:{CLSID}" -> L"{CLSID}"
    std::string clsid = fileName.substr(6);
    return std::wstring(clsid.begin(), clsid.end());
}

std::wstring StripLnkExtension(const std::wstring& name) {
    if (name.length() > 4) {
        std::wstring ext = name.substr(name.length() - 4);
        if (_wcsicmp(ext.c_str(), L".lnk") == 0) {
            return name.substr(0, name.length() - 4);
        }
    }
    return name;
}

} // namespace IconUtils
