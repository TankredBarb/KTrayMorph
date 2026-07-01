# KTrayMorph Roadmap

## Project Goal

KTrayMorph is a KDE/Linux utility for replacing system tray icons with user-selected icons. It should support both regular tray applications and KDE Plasma's built-in tray items, such as volume, network, battery, brightness, clipboard, notifications, and media controls.

The application should feel native in KDE Plasma, follow the active Plasma theme, keep QML focused on presentation, and keep the main logic in C++ for performance and maintainability.

## Core Assumptions

- Target platform: KDE Plasma 6 on Linux.
- Main stack: Qt 6, KF6, CMake, C++.
- UI stack: QML, Kirigami, Plasma Components, with minimal JavaScript.
- Build mode: release builds by default for normal local usage.
- System files under `/usr/share/plasma` must not be modified.
- Global icon themes must not be patched as the primary replacement mechanism.
- Icon replacements must be user-scoped, reversible, and safe to disable.

## Important Technical Finding

Tray icons fall into two different groups.

1. **External StatusNotifierItem icons**

   Examples: Telegram, Yakuake, update notifiers, wallet manager, Discord, Signal.

   These are exposed through `org.kde.StatusNotifierWatcher` and `org.kde.StatusNotifierItem` over D-Bus. They can expose icon data as `IconName`, `IconPixmap`, or both. Some apps, such as Telegram, may expose a pixmap icon without a stable icon name.

2. **Internal Plasma tray applets**

   Examples: `org.kde.plasma.volume`, `org.kde.plasma.networkmanagement`, `org.kde.plasma.battery`, `org.kde.plasma.brightness`.

   These are not always external StatusNotifierItem services. They can be internal Plasma applets hosted by `org.kde.plasma.systemtray`. Replacing them requires Plasma-level integration, not only a D-Bus StatusNotifier proxy.

This means KTrayMorph should not start by replacing the KDE StatusNotifier watcher. That approach is risky and can break tray behavior. The safer first architecture is a Plasma-level applet plus a C++ QML plugin that can observe, match, and override icons in the tray presentation layer.

## Proposed Architecture

### Plasma Applet

The applet is responsible for integration with the Plasma tray UI. It should:

- locate the active `org.kde.plasma.systemtray` instance when possible;
- observe tray delegate items;
- expose internal Plasma tray items to the C++ model;
- apply icon replacements in the presentation layer;
- preserve original click, hover, menu, popup, and attention behavior.

### C++ QML Plugin

The C++ plugin owns the main logic. It should provide:

- D-Bus StatusNotifier discovery;
- tray item models;
- rule matching;
- icon resolving;
- pixmap hashing;
- configuration loading and saving;
- caching;
- diagnostics.

### Configuration UI

The UI should be a native KDE settings surface using Kirigami and Plasma controls. It should provide:

- detected tray items;
- replacement rules;
- theme icon picker;
- local file picker;
- diagnostics;
- enable/disable controls.

### Optional Helper Process

An external helper process should only be added if the applet and plugin cannot reliably handle discovery or persistence. It is not part of the initial design.

## Data Model

### Tray Item

Each visible or known tray item should be represented as a normalized item:

```json
{
  "stableId": "org.kde.plasma.volume",
  "kind": "PlasmaApplet",
  "title": "Audio Volume",
  "iconName": "audio-volume-high-symbolic",
  "iconHash": "",
  "status": "Active",
  "replacementIcon": ""
}
```

For StatusNotifierItem entries:

```json
{
  "stableId": "Yakuake",
  "kind": "StatusNotifier",
  "service": ":1.39",
  "path": "/StatusNotifierItem",
  "sniId": "Yakuake",
  "title": "Yakuake",
  "category": "ApplicationStatus",
  "iconName": "yakuake-symbolic",
  "iconHash": "",
  "status": "Active",
  "replacementIcon": ""
}
```

## Replacement Rules

Initial rule format:

```json
{
  "enabled": true,
  "description": "Audio volume",
  "matchType": "stableId",
  "matchValue": "org.kde.plasma.volume",
  "replacementType": "themeIcon",
  "replacementValue": "audio-volume-high-symbolic"
}
```

Supported match types for the first implementation:

- `stableId`
- `pluginName`
- `sniId`
- `title`
- `iconName`
- `iconHash`
- `servicePath`

Supported replacement types:

- `themeIcon`
- `localFile`

Current prototype status:

- `themeIcon` is implemented as the current replacement value behavior.
- `localFile` has a first-pass implementation for SVG/PNG paths.
- Existing rules without `replacementType` are interpreted as `themeIcon`.

Rule behavior:

- disabled rules are ignored;
- rules are evaluated from top to bottom;
- the first matching rule wins;
- exact matching is preferred for the initial implementation;
- fuzzy matching should not be added until the exact matching path is stable.

## Phase 1: Project Skeleton

Goal: create a minimal buildable Qt 6/KF6 project.

Tasks:

- create the CMake project;
- add a minimal C++ QML plugin;
- add a minimal Plasma applet package;
- add a minimal configuration UI shell;
- use release build settings by default for normal local builds.

Verification:

- CMake configures successfully;
- the project builds in release mode;
- the QML plugin can be loaded;
- the applet package is installable or testable through Plasma tooling.

## Phase 2: StatusNotifierItem Discovery

Goal: detect regular external tray applications.

Tasks:

- read `org.kde.StatusNotifierWatcher.RegisteredStatusNotifierItems`;
- subscribe to item registration and unregistration signals;
- read common `org.kde.StatusNotifierItem` properties:
  - `Id`;
  - `Title`;
  - `Category`;
  - `Status`;
  - `IconName`;
  - `IconPixmap`;
  - `OverlayIconName`;
  - `AttentionIconName`;
  - `ToolTip`;
- expose detected SNI items through a C++ model.

Verification:

- Telegram, Yakuake, update notifier, or wallet manager appear when present;
- items disappear when the source app exits;
- items update when their icon changes;
- pixmap-only icons are detected as pixmap-only instead of being treated as missing icons.

## Phase 3: Icon Hashing

Goal: support applications that expose pixmaps instead of stable icon names.

Tasks:

- parse StatusNotifierItem `IconPixmap`;
- normalize pixmaps to a consistent image format;
- hash pixmap contents;
- cache icon hashes;
- update hashes only when the source icon changes.

Verification:

- pixmap-only apps receive non-empty icon hashes;
- repeated reads of the same icon produce the same hash;
- icon state changes produce different hashes when the visual icon changes;
- idle CPU usage remains near zero.

## Phase 4: Plasma Tray Item Discovery

Goal: detect internal Plasma tray items such as volume and network.

Tasks:

- locate the active `org.kde.plasma.systemtray` instance;
- inspect the tray model/delegates from the Plasma applet layer;
- identify `StatusNotifier` and `Plasmoid` item types;
- collect internal applet properties:
  - plugin name;
  - title;
  - icon name;
  - status;
  - compact representation state where available;
- add a Plasma-version adapter layer.

Primary target plugin names:

- `org.kde.plasma.volume`
- `org.kde.plasma.networkmanagement`
- `org.kde.plasma.battery`
- `org.kde.plasma.brightness`
- `org.kde.plasma.bluetooth`
- `org.kde.plasma.clipboard`
- `org.kde.plasma.mediacontroller`
- `org.kde.plasma.notifications`
- `org.kde.plasma.devicenotifier`

Verification:

- volume and network are detected in the current tray;
- hidden tray items can be detected;
- the tray expander is handled separately;
- discovery recovers after `plasmashell` restarts.

## Phase 5: Rule Engine

Goal: match tray items to replacement icons.

Tasks:

- implement a small C++ rule type;
- load and save rules as JSON;
- match rules against normalized tray items;
- expose matched replacement data to QML;
- support enable/disable and ordering.

Verification:

- a rule for `org.kde.plasma.volume` matches the volume item;
- a rule for `Yakuake` matches the Yakuake SNI item;
- a hash-based rule matches a pixmap-only icon;
- disabling a rule restores the original icon.

## Phase 6: Icon Replacement

Goal: replace displayed icons without breaking original tray behavior.

Tasks:

- resolve theme icon replacements through KDE icon theme APIs;
- resolve local SVG/PNG replacements;
- apply replacement icons to SNI delegates;
- apply replacement icons to internal Plasma applet delegates;
- preserve original activation, context menu, popup, and attention behavior.

Verification:

- replacing Yakuake's icon keeps its activation/menu behavior working;
- replacing the volume icon keeps the volume popup working;
- replacing the network icon keeps the network popup working;
- replacements survive normal icon state changes;
- all replacements can be disabled globally.

## Phase 7: Native Configuration UI

Goal: make rule creation and editing practical.

Screens:

- Detected Items
- Rules
- Icon Picker
- Diagnostics

Detected Items should show:

- current icon preview;
- item kind;
- title;
- stable id;
- icon name;
- icon hash when available;
- a command to create a rule from the item.

Rules should support:

- add;
- edit;
- duplicate;
- delete;
- enable/disable;
- reorder.

Icon Picker should support:

- searching the current KDE icon theme;
- previewing on light and dark backgrounds;
- selecting a local SVG or PNG file.

Diagnostics should show:

- Plasma version;
- whether the system tray was found;
- number of SNI items;
- number of Plasma applet items;
- recent replacement errors.

Verification:

- a user can create a working rule from a detected item;
- a user can choose a theme icon;
- a user can choose a local file icon;
- configuration persists after restart;
- the UI follows light and dark KDE themes.

Current status:

- The applet has a practical detected-items view.
- Theme icon search/picker is implemented with async search and previews.
- Local SVG/PNG file selection is implemented as a header action next to icon search.
- Per-row create/update and undo are implemented.
- Clear-all and refresh are implemented.
- Standard Plasma `Configure KTrayMorph` integration exists for logging and polling settings.
- A full rules editor is not implemented yet.
- Missing-file repair UI is not implemented yet.

## Phase 7.1: Custom User Icons

Goal: allow users to use their own icon files, not only installed system/theme icons.

Current status: first-pass local SVG/PNG selection and missing-file UI are implemented. This phase remains open for rule editing and repair-flow polish.

Supported file types for the first pass:

- SVG
- PNG

Tasks:

- add a local file picker as a header action next to icon search: implemented;
- preview selected local files in the row replacement preview: implemented;
- store replacement type explicitly as `themeIcon` or `localFile`: implemented;
- migrate or interpret existing rules without `replacementType` as `themeIcon`: implemented;
- validate that selected local files exist and are readable: implemented in the rule engine;
- apply local files through the same live replacement path used for theme icon names: implemented;
- preserve restore behavior exactly as today: implemented through the existing live replacement lifecycle;
- keep local file paths user-scoped and avoid copying files into system icon themes: implemented;
- add clear missing-file UI states: implemented;
- add editing support for changing a saved rule between `themeIcon` and `localFile`: pending.

Rule example:

```json
{
  "enabled": true,
  "matchType": "stableId",
  "matchValue": "org.kde.plasma.clipboard",
  "replacementType": "localFile",
  "replacementValue": "/home/user/.local/share/icons/custom/clipboard.svg"
}
```

Verification:

- a user can choose a local SVG file and apply it to an internal Plasma tray applet;
- a user can choose a local PNG file and apply it to a StatusNotifierItem;
- existing theme-icon rules still work after the rule schema change;
- missing local files produce a clear UI state instead of a broken replacement;
- removing the widget restores original icons;
- reboot/login keeps the rule and re-applies it while the widget is active.

## Phase 8: Performance Pass

Goal: keep KTrayMorph cheap to run.

Current decision:

- Continuous polling is used deliberately because dynamic Plasma applets can swap or redraw their icon targets without a useful signal for KTrayMorph.
- The polling interval is configurable, defaults to 850ms, and is clamped to 250-5000ms.
- The polling tick exits before scanning the tray tree when there are no replacement rules and no live replacement records.
- SNI D-Bus scanning is not performed on every polling tick; SNI items are cached and refreshed through normal refresh/SNI-change paths.

Rules:

- keep the polling path narrow;
- prefer D-Bus and Plasma signals for discovery where they are reliable;
- hash icons only when icon data changes;
- cache resolved icons;
- cache local files by path and modification time;
- keep QML bindings simple;
- keep rule matching in C++.

Verification:

- idle CPU usage is effectively zero when no replacement rules exist;
- memory does not grow during repeated icon changes;
- frequent icon updates do not visibly freeze Plasma.

## Phase 9: Robustness

Goal: avoid breaking Plasma even when internals change.

Tasks:

- isolate Plasma-private access in adapter classes;
- add graceful fallbacks when objects are missing;
- log clear diagnostics;
- avoid throwing fatal errors into QML;
- support restarting `plasmashell`;
- add focused tests for rule matching and icon hashing.

Verification:

- missing Plasma internals disable only affected functionality;
- external SNI discovery can still work if internal Plasma discovery fails;
- the applet does not crash Plasma on unsupported versions;
- tests cover rule priority and exact matching.

## Phase 10: Packaging

Goal: provide a clean local install path.

Tasks:

- install the Plasma applet package;
- install the QML plugin;
- install metadata;
- document build and install commands;
- document uninstall commands;
- keep user settings in `~/.config/ktraymorph/`.

Expected local build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo ./install.sh
```

Verification:

- clean install works;
- uninstall removes installed project files;
- user configuration remains user-scoped;
- no system Plasma files are patched.

## First Milestone

The first milestone should prove the riskiest part of the project:

> KTrayMorph prototype replaces one external StatusNotifierItem icon and one internal Plasma tray icon, survives a `plasmashell` restart, and exposes both items in a minimal native configuration UI.

Status: reached for the tested current Plasma session.

Milestone scope:

- minimal CMake project;
- C++ QML plugin;
- minimal Plasma applet;
- SNI discovery;
- Plasma tray discovery;
- JSON rules;
- replacement for one SNI item, such as Yakuake;
- replacement for one internal Plasma item, such as volume or network;
- minimal settings UI;
- manual verification on the current KDE session.

## Immediate Next Steps

1. Add a small rules editor for existing saved rules.
2. Add editing support for switching an existing rule between `themeIcon` and `localFile`.
3. Add a repair flow for missing local files.
4. Add uninstall documentation.
5. Add focused tests for rule matching, schema compatibility, and icon hashing.
