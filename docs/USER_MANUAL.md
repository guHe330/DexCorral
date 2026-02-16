# User Manual

DexCorral is a native C++ utility designed to help you organize your Windows desktop icons into dedicated areas called "Corrals".

## Core Concepts

### Corrals
A Corral is a semi-transparent window that sits on your desktop. It acts as a container for specific desktop icons. When an icon is assigned to a Corral, it is moved to a position behind the Corral window.

### Catch-All Corral
The application maintains one "Catch-All" Corral. Any new files or shortcuts added to your desktop are automatically moved to this Corral to prevent desktop clutter.

## Getting Started

1.  Launch `DexCorral.exe`.
2.  Upon first run, a default "Desktop" Corral is created in the center of your screen as the Catch-All.
3.  Your desktop icons will be managed according to the configuration.

## Managing Corrals

### Creating a new Corral
*   Right-click the DexCorral tray icon.
*   Select "Create New Corral".
*   A new Corral will appear on your desktop.

### Moving and Resizing
*   Click and drag the title bar or the body of a Corral to move it.
*   Use the edges of the Corral window to resize it.

### Customizing a Corral
Right-click on a Corral's title bar or background to access customization options:
*   **Rename**: Change the title of the Corral.
*   **Change Color**: Select a new background color for the Corral.
*   **Set as Catch-All**: Designate this Corral as the one to receive all new desktop items.
*   **Delete Corral**: Remove the Corral. Icons inside will remain on the desktop but will no longer be grouped.

### Roll-up Feature
Double-click the title bar of a Corral to "roll it up". This hides the content area and only shows the title bar, saving space on your desktop. Double-click again to expand.

## Working with Icons

### Adding Icons to a Corral
*   Drag an icon from the desktop and drop it onto a Corral window.
*   The icon is now tracked by that Corral and will stay positioned behind it.

### Removing Icons from a Corral
*   Right-click an icon inside a Corral.
*   Select "Remove from Corral".
*   The icon will stay on the desktop but is no longer associated with that specific Corral.

## Tray Icon Options

The tray icon provides quick access to global settings:
*   **Create New Corral**: Quickly add a container to your desktop.
*   **Show Desktop Icons**: Toggle the visibility of all desktop icons (native Windows icons).
*   **Start with Windows**: Enable or disable automatic startup when you log in.
*   **Exit**: Close the application. All icons will remain in their last positions on the desktop.

## Configuration

DexCorral saves its configuration (Corral positions, titles, colors, and assigned files) in a JSON file located at:
`%APPDATA%\DexCorralCpp\config.json`
