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
