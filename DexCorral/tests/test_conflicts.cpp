/**
 * test_conflicts.cpp - Unit tests for the identity checks in Conflicts.h.
 *
 * Only the pure string logic is covered: which module names, version resources
 * and "installed programs" entries count as Stardock Fences. The values used
 * for the positive cases are the ones Fences 6.5.4.1 reports.
 */

#include <gtest/gtest.h>
#include "Conflicts.h"

TEST(FencesModuleName, AllThreeArchitectures) {
    EXPECT_TRUE(IsFencesModuleName(L"DesktopDock64.dll"));
    EXPECT_TRUE(IsFencesModuleName(L"DesktopDockARM64.dll"));
    EXPECT_TRUE(IsFencesModuleName(L"DesktopDock.dll"));
}

TEST(FencesModuleName, CaseInsensitive) {
    EXPECT_TRUE(IsFencesModuleName(L"desktopdock64.DLL"));
    EXPECT_TRUE(IsFencesModuleName(L"DESKTOPDOCK.DLL"));
}

TEST(FencesModuleName, OtherModulesAreNotMatched) {
    EXPECT_FALSE(IsFencesModuleName(L""));
    EXPECT_FALSE(IsFencesModuleName(L"FencesMenu64.dll")) << "only the desktop module is the trigger";
    EXPECT_FALSE(IsFencesModuleName(L"DexCorralHook.dll"));
    EXPECT_FALSE(IsFencesModuleName(L"DesktopDock64.dll.old"));
    EXPECT_FALSE(IsFencesModuleName(L"C:\\Program Files (x86)\\Stardock\\Fences\\DesktopDock64.dll"))
        << "takes a file name, not a path";
    EXPECT_FALSE(IsFencesModuleName(L"MyDesktopDock64.dll"));
}

TEST(FencesVersionInfo, ValuesReportedByTheRealModule) {
    EXPECT_TRUE(IsFencesVersionInfo(L"Stardock", L"Stardock Fences"));
}

TEST(FencesVersionInfo, ToleratesCaseAndLongerCompanyNames) {
    EXPECT_TRUE(IsFencesVersionInfo(L"Stardock Software, Inc.", L"Fences"));
    EXPECT_TRUE(IsFencesVersionInfo(L"STARDOCK SYSTEMS, INC.", L"stardock fences desktop module"));
}

TEST(FencesVersionInfo, NeedsBothCompanyAndProduct) {
    // Same file name from someone else, or another Stardock product, is not a conflict
    EXPECT_FALSE(IsFencesVersionInfo(L"Some Other Company", L"Desktop Dock"));
    EXPECT_FALSE(IsFencesVersionInfo(L"Stardock", L"ObjectDock"));
    EXPECT_FALSE(IsFencesVersionInfo(L"Someone", L"Garden Fences Planner"));
    EXPECT_FALSE(IsFencesVersionInfo(L"", L""));
}

TEST(FencesInstallEntry, EntryWrittenByTheRealInstaller) {
    EXPECT_TRUE(IsFencesInstallEntry(L"Stardock Fences 6", L"Stardock Software, Inc."));
}

TEST(FencesInstallEntry, PublisherCanSupplyTheVendor) {
    EXPECT_TRUE(IsFencesInstallEntry(L"Fences 5", L"Stardock Software, Inc."));
    EXPECT_TRUE(IsFencesInstallEntry(L"Stardock Fences", L""));
}

TEST(FencesInstallEntry, OtherProductsAreNotMatched) {
    EXPECT_FALSE(IsFencesInstallEntry(L"Stardock Start11", L"Stardock Software, Inc."));
    EXPECT_FALSE(IsFencesInstallEntry(L"Garden Fences Planner", L"Some Other Company"));
    EXPECT_FALSE(IsFencesInstallEntry(L"DexCorral", L"DexCorral"));
    EXPECT_FALSE(IsFencesInstallEntry(L"", L"Stardock Software, Inc."));
    EXPECT_FALSE(IsFencesInstallEntry(L"", L""));
}
