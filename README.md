# KTrayMorph

KTrayMorph is a KDE Plasma 6 applet for replacing visible system tray icons with user-selected theme icons or local SVG/PNG files.

It is intentionally a Plasma-level tray UI hack: instead of patching icon themes or replacing the StatusNotifier watcher, it observes the active Plasma system tray presentation tree and repeatedly applies the configured icon overrides.

## Current Status

This is an early prototype. It works in the tested Plasma 6 session, but it depends on Plasma's private QML object structure, so future Plasma updates can break tray discovery or replacement targeting.

Supported replacement targets:

- Plasma internal tray applets, such as volume, brightness, clipboard, keyboard layout, and media controls.
- StatusNotifierItem tray icons with a stable icon name, such as Yakuake.
- StatusNotifierItem tray icons that expose pixmap data, such as Telegram.

Placement behavior:

- For Plasma internal tray applets, KTrayMorph must be placed on the same panel as the Plasma System Tray.
- On the desktop, KTrayMorph can still show and manage StatusNotifierItem rows, but it cannot inspect internal Plasma tray delegates.
- On a panel without a same-panel System Tray, the expanded view shows a placement warning instead of the full UI.

## Features

- Persisted per-item replacement rules.
- Theme icon replacement.
- Local SVG/PNG replacement.
- Polling-based reassertion for dynamic tray applets whose icon changes with state.
- Optional diagnostic logging.
- Configurable polling interval.

## Build

Requirements:

- KDE Plasma 6
- Qt 6.6 or newer
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

- logging enable/disable;
- log file path;
- polling interval in milliseconds.

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
