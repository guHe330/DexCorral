/**
 * test_accent_color.cpp - Unit tests for AccentColor.
 *
 * How a tab that follows the Windows accent colour is coloured.
 */

#include <gtest/gtest.h>
#include "AccentColor.h"

TEST(AccentColor, RegistryValueIsBlueGreenRed)
{
    // HKCU\Software\Microsoft\Windows\DWM\AccentColor for the default Windows blue
    EXPECT_EQ(AccentColor::RgbFromRegistry(0xFFD47800u), 0x0078D4u);
}

TEST(AccentColor, BackgroundKeepsTheTabsTransparency)
{
    EXPECT_EQ(AccentColor::Background("#99102030", 0x0078D4u), "#990078D4");
}

TEST(AccentColor, BackgroundOfAMalformedColourUsesTheDefaultTransparency)
{
    EXPECT_EQ(AccentColor::Background("", 0x0078D4u), "#990078D4");
    EXPECT_EQ(AccentColor::Background("#FFF", 0x0078D4u), "#990078D4");
}

TEST(AccentColor, TextIsWhiteOnADarkAccent)
{
    EXPECT_EQ(AccentColor::TextOn(0x0078D4u), "#FFFFFF"); // Windows blue
    EXPECT_EQ(AccentColor::TextOn(0x000000u), "#FFFFFF");
    EXPECT_EQ(AccentColor::TextOn(0x744DA9u), "#FFFFFF"); // Windows violet
}

TEST(AccentColor, TextIsBlackOnALightAccent)
{
    EXPECT_EQ(AccentColor::TextOn(0xFFB900u), "#000000"); // Windows yellow
    EXPECT_EQ(AccentColor::TextOn(0xFFFFFFu), "#000000");
}

TEST(AccentColor, TextIsBlackOnAMidBrightAccent)
{
    EXPECT_EQ(AccentColor::TextOn(0x00B7C3u), "#000000"); // Windows teal
    EXPECT_EQ(AccentColor::TextOn(0x00CC6Au), "#000000"); // Windows green
    EXPECT_EQ(AccentColor::TextOn(0xFF8C00u), "#000000"); // Windows orange
    EXPECT_EQ(AccentColor::TextOn(0x107C10u), "#FFFFFF"); // Dark green stays white
}
