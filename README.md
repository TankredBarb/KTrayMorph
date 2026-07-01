# KTrayMorph

KTrayMorph is a KDE Plasma 6 applet for replacing visible system tray icons with user-selected theme icons, local SVG/PNG files, or color tint overlays.

It is intentionally a Plasma-level tray UI hack: instead of patching icon themes or replacing the StatusNotifier watcher, it observes the active Plasma system tray presentation tree and keeps configured visual overrides applied while the widget is active.

## Current Status

This is an early prototype. It works in the tested Plasma 6 session, but it depends on Plasma's private QML object structure, so future Plasma updates can break tray discovery or replacement targeting.

Supported replacement targets:

- Plasma internal tray applets, such as volume, brightness, clipboard, keyboard layout, and media controls.
- StatusNotifierItem tray icons with a stable icon name, such as Yakuake.
- StatusNotifierItem tray icons that expose pixmap data, such as Telegram. Pixmap-only items support icon replacement, but not color tint.

Placement behavior:

- For Plasma internal tray applets, KTrayMorph must be placed on the same panel as the Plasma System Tray.
- On the desktop, KTrayMorph can still show and manage StatusNotifierItem rows, but it cannot inspect internal Plasma tray delegates.
- On a panel without a same-panel System Tray, the expanded view shows a placement warning instead of the full UI.

## Features

- Persisted per-item replacement rules.
- Theme icon replacement.
- Local SVG/PNG replacement.
- Color tint replacement for named/theme icons.
- Overlay-based live replacement that does not mutate the native tray icon binding.
- Current-state restore when replacements are disabled, rules are removed, or the widget is removed.
- Polling-based reassertion for dynamic tray applets whose icon changes with state, such as media controls and volume.
- Optional diagnostic logging.
- Configurable polling interval.
- Quick Enable/Disable action in the applet context menu.

## How It Works

KTrayMorph creates its own icon overlay above the native tray icon and hides the native visual item. The original Plasma or StatusNotifier icon continues to exist underneath, so restore is handled by removing the overlay and revealing the current native icon state.

This matters for dynamic tray applets. For example, media controls can switch between play, pause, stop, and application icons, and volume can switch between level-specific icons. KTrayMorph deliberately reasserts active rules on a timer because those applets can redraw or swap their visible icon target without a reliable public signal.

The default polling interval is 850ms and can be changed in the applet settings. Polling exits early when there are no replacement rules and no live replacement records.

Replacement types:

- `themeIcon`: use an icon from the current icon theme.
- `localFile`: use a user-selected SVG or PNG file.
- `colorTint`: recolor the current named/theme icon through an overlay. Pixmap-only StatusNotifier icons are excluded from tinting because they do not expose a stable theme source to recolor reliably.

## Build

Requirements:

- KDE Plasma 6
- Qt 6.6 or newer
- Qt 6 5Compat QML module (`qt6-5compat` on Arch/CachyOS)
- KDE Frameworks 6
- CMake 3.25 or newer
- Extra CMake Modules

Build locally:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Install

The development install script builds the project, installs the QML plugin and applet system-wide, installs/upgrades the Plasma applet globally, and restarts Plasma:

```bash
sudo ./install.sh
```

The script must be run as root because it installs system files and globally upgrades the Plasma applet.

## Configuration

Rules are stored in:

```text
~/.config/ktraymorph/rules.json
```

The applet settings provide:

- active enable/disable for all replacements;
- logging enable/disable;
- log file path;
- polling interval in milliseconds.

When inactive, KTrayMorph restores live overrides, stops polling, disables the main UI, and keeps saved rules ready for reactivation.

The applet context menu also exposes a quick enable/disable action. The applet icon is desaturated while replacements are inactive.

## Logging

Logging is disabled by default. When enabled, KTrayMorph writes diagnostic information to the configured log file. The log is intended for investigating Plasma tray targeting issues and dynamic applet icon changes, not for normal operation.

## Diagnostic Tool

After building, the StatusNotifier diagnostic tool can be run with:

```bash
build/bin/ktraymorph-dump-sni
```

For temporary rule testing:

```bash
KTRAYMORPH_RULES_FILE=/tmp/ktraymorph-rules.json build/bin/ktraymorph-dump-sni
```

## License

LGPL-3.0-or-later.
