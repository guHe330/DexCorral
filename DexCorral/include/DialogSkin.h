/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * DialogSkin.h - One look for every DexCorral dialog
 *
 * Follows the light or dark app mode of Windows: a plain body, a footer band
 * for the buttons, rounded buttons with the default one in the accent colour,
 * and check boxes and track bars drawn in the accent colour. The controls stay
 * the standard ones (keyboard, screen readers); only their painting changes.
 *
 * With a high-contrast theme the skin stays off and Windows draws everything.
 *
 * Use: call Attach in WM_INITDIALOG and give HandleMessage every message the
 * dialog procedure does not consume itself:
 *
 *     INT_PTR skinned;
 *     if (DialogSkin::HandleMessage(hDlg, msg, wParam, lParam, skinned))
 *         return skinned;
 */

#pragma once
#include <Windows.h>
#include <string>

namespace DialogSkin
{
/// -1: follow Windows. 0 / 1: light / dark regardless (previews)
extern int forceTheme;

/// The value of forceTheme for the Theme setting of the config ("", "light", "dark");
/// anything else follows Windows
inline int ThemeOverride(const std::string &theme)
{
    return theme == "light" ? 0 : theme == "dark" ? 1 : -1;
}

/// True when dialogs are to be dark
bool IsDark();

/// The colours of the current mode, for whoever draws along with the skin (menus)
struct Palette
{
    COLORREF body, footer, line, text, textMuted, textDisabled;
    COLORREF button, buttonHover, buttonPressed, buttonBorder;
    COLORREF field;  // Edit and list backgrounds
    COLORREF accent; // The Windows accent colour
    COLORREF onAccent;
};
Palette CurrentPalette();

/// Fills `r` with a rounded rectangle whose edge is smoothed against `behind`
void FillRoundRect(HDC dc, const RECT &r, int radius, COLORREF fill, COLORREF border, COLORREF behind);

/// Skins the dialog and its controls. footerTopDlu: where the footer band
/// with the buttons begins, in dialog units; negative for no footer.
void Attach(HWND hDlg, int footerTopDlu);

/// True if the skin answered the message; return `result` from the dialog procedure then
bool HandleMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam, INT_PTR &result);

/// A static text as an explanation (muted colour)
void MakeHint(HWND hDlg, int id);

/// A push button as one entry of a page strip. Call again to change `selected`.
void MakePageTab(HWND hDlg, int id, bool selected);
} // namespace DialogSkin
