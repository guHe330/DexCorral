DexCorral MSIX Assets
======================

This directory contains the visual assets required for the MSIX package.

Required Assets:
----------------

1. Square44x44Logo.png (44x44 pixels)
   - Small logo for app list and taskbar
   - Should be a simple, recognizable icon

2. Square150x150Logo.png (150x150 pixels)
   - Medium tile logo for Start menu
   - Main logo representation

3. Wide310x150Logo.png (310x150 pixels)
   - Wide tile logo for Start menu
   - Horizontal layout of logo/branding

4. StoreLogo.png (50x50 pixels)
   - Logo for Microsoft Store (if publishing)
   - Simplified version of main logo

Asset Guidelines:
-----------------
- Use transparent backgrounds (PNG with alpha channel)
- Follow Microsoft's design guidelines for app icons
- Keep designs simple and scalable
- Use consistent color scheme across all assets
- Test on both light and dark backgrounds

Creating Assets:
----------------
You can create these assets using:
- Adobe Photoshop/Illustrator
- Figma/Sketch
- GIMP (free alternative)
- Online tools like Canva

For DexCorral, consider using imagery related to:
- Desktop organization
- Corrals
- Grid/layout symbols
- File/folder management

Quick Placeholder Generation:
------------------------------
For testing purposes, you can generate simple placeholder images using
PowerShell or any image editing tool. The build script will warn if
real assets are missing.

Example using ImageMagick:
  magick -size 44x44 xc:#1E90FF Square44x44Logo.png
  magick -size 150x150 xc:#1E90FF Square150x150Logo.png
  magick -size 310x150 xc:#1E90FF Wide310x150Logo.png
  magick -size 50x50 xc:#1E90FF StoreLogo.png
