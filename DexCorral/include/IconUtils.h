/**
 * IconUtils.h - Pure string utilities for icon entry classification and name normalization.
 *
 * These functions are deliberately free of Win32 API calls so they can be compiled
 * into the unit test executable without dragging in window/shell dependencies.
 */

#pragma once
#include <string>

namespace IconUtils {

    /// Returns true if the entry is a special shell icon ("shell:{CLSID}").
    bool IsSpecialIconEntry(const std::string& fileName);

    /// Extracts the CLSID from a "shell:{CLSID}" entry, returning L"{CLSID}".
    std::wstring GetSpecialIconClsid(const std::string& fileName);

    /// Strips a ".lnk" extension (case-insensitive) from a wide filename.
    /// If the name doesn't end in ".lnk" or is too short, it is returned unchanged.
    std::wstring StripLnkExtension(const std::wstring& name);

} // namespace IconUtils
