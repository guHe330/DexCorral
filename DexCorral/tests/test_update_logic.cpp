// Unit tests for the update check's pure logic (UpdateLogic.h): version
// parsing/comparison and the allow-list for the release URL that ends up in
// ShellExecuteW.

#include <gtest/gtest.h>
#include "UpdateLogic.h"

using namespace UpdateLogic;

TEST(UpdateLogic, ParsesVersionWithAndWithoutPrefix)
{
    Version v = ParseVersion("v1.2.3");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);

    v = ParseVersion("10.0.29");
    EXPECT_EQ(v.major, 10);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 29);
}

TEST(UpdateLogic, ParsesPartialAndSuffixedVersions)
{
    Version v = ParseVersion("V2.5");
    EXPECT_EQ(v.major, 2);
    EXPECT_EQ(v.minor, 5);
    EXPECT_EQ(v.patch, 0);

    v = ParseVersion("1.2.3-beta4");
    EXPECT_EQ(v.patch, 3);

    v = ParseVersion("garbage");
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST(UpdateLogic, NewerIsStrict)
{
    EXPECT_TRUE(IsNewer(ParseVersion("1.1.3"), ParseVersion("1.1.2")));
    EXPECT_TRUE(IsNewer(ParseVersion("1.2.0"), ParseVersion("1.1.9")));
    EXPECT_TRUE(IsNewer(ParseVersion("2.0.0"), ParseVersion("1.9.9")));
    EXPECT_TRUE(IsNewer(ParseVersion("1.10.0"), ParseVersion("1.9.0"))); // numeric, not textual
    EXPECT_FALSE(IsNewer(ParseVersion("1.1.2"), ParseVersion("1.1.2")));
    EXPECT_FALSE(IsNewer(ParseVersion("1.1.1"), ParseVersion("1.1.2")));
    EXPECT_FALSE(IsNewer(ParseVersion("garbage"), ParseVersion("0.0.1")));
}

TEST(UpdateLogic, TrustsOnlyTheProjectsOwnReleaseUrls)
{
    EXPECT_TRUE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/releases/tag/v1.2.0"));
    EXPECT_TRUE(IsTrustedReleaseUrl("HTTPS://GitHub.com/guHe330/DexCorral/releases/latest"));

    EXPECT_FALSE(IsTrustedReleaseUrl(""));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/")); // nothing after the prefix
    EXPECT_FALSE(IsTrustedReleaseUrl("http://github.com/guHe330/DexCorral/releases/latest"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/someoneelse/DexCorral/releases/latest"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com@evil.example/guHe330/DexCorral/releases"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com.evil.example/guHe330/DexCorral/releases"));
    EXPECT_FALSE(IsTrustedReleaseUrl("file:///C:/Windows/System32/calc.exe"));
    EXPECT_FALSE(IsTrustedReleaseUrl("\\\\server\\share\\x.exe"));
}

TEST(UpdateLogic, RejectsTraversalAndOddCharacters)
{
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/../../evil/repo"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/releases/a b"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/releases/\n"));
    EXPECT_FALSE(IsTrustedReleaseUrl("https://github.com/guHe330/DexCorral/releases/\xC3\xA4"));
}