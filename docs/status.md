# KTrayMorph Status

Last updated: 2026-07-01

## Current Position

KTrayMorph is now a working KDE Plasma 6 prototype for replacing visible system tray icons from a Plasma applet.

It can replace and restore:

- KDE Plasma internal tray applet icons, such as clipboard, brightness, audio, keyboard layout, and similar same-panel tray applets.
- StatusNotifierItem icons with a stable theme icon name, such as Yakuake.
- StatusNotifierItem icons that expose only pixmap data, such as Telegram.

Rules are persisted in the user's config and are re-applied while the KTrayMorph widget is active. Live visual changes are removed when the widget is disabled, destroyed, or removed from the panel.

Live replacement state is owned by a C++ `LiveReplacementController` (`src/livereplacementcontroller.h/cpp`, exposed to QML as `org.ktraymorph.core.LiveReplacementController`). QML only inspects the Plasma system-tray QML tree to locate icon targets and forwards them to the controller. The controller persists original/replacement sources, reasserts replacement on drift, and restores the original on undo/widget teardown.

Current live replacement uses an overlay model. KTrayMorph creates its own `Kirigami.Icon` overlay above the native tray icon and hides the native visual item by opacity. It does not mutate the native icon's `source` binding. Restore removes the overlay and reveals the native tray icon in its current state.

Replacement reassert currently uses an explicit polling approach:

- Polling layer: an active root QML timer re-scans tray delegates, reapplies persisted replacements, and calls `LiveReplacementController::reassertAll()` at the configured interval when replacements exist. The default is 850ms, clamped to 250-5000ms. This intentionally favors reliability over event purity because dynamic Plasma applets can swap their visible icon target without a useful signal for KTrayMorph.
- Burst layer: enabling replacements, saving a rule, or retargeting schedules a short burst of reapply passes at 50/120/250/500/900ms. This suppresses immediate redraws from dynamic applets such as media controls and volume.
- Idle behavior: when there are no replacement rules and no live replacement records, the polling tick exits before scanning the tray tree.
- Diagnostics: when logging is enabled, KTrayMorph logs observed changes to replacement applets' `applet.plasmoid.icon` and direct visual `source` values. This is used to investigate whether dynamic applets expose their intended native icon while overrides are active.

The current implementation is still a prototype because it relies on Plasma QML tree inspection for presentation-layer replacement. It is working in the current tested Plasma session, but Plasma-private object structure can change between Plasma versions.

## Implemented

### Build And Install

Implemented files:

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `plasmoid/CMakeLists.txt`
- `install.sh`

Current behavior:

- Builds a Qt 6/KF6 QML plugin: `org.ktraymorph.core`.
- Builds the Plasma applet package: `org.ktraymorph.plasmoid`.
- Installs system files through `cmake --install build`.
- Installs or upgrades the Plasma applet globally through `kpackagetool6 --type Plasma/Applet --global`.
- Removes user-local applet copies before installing, so stale `~/.local/share/plasma/plasmoids/org.ktraymorph.plasmoid` does not shadow the global install.
- Requires the Qt 6 5Compat GraphicalEffects QML module (`qt6-5compat` on Arch/CachyOS) for color tint rendering.
- Restarts Plasma after install.

Normal local install:

```bash
sudo ./install.sh
```

### Applet Metadata

Implemented file:

- `plasmoid/package/metadata.json`

Current metadata:

- Name: `KTrayMorph`
- Plugin id: `org.ktraymorph.plasmoid`
- Author: `tankred <tankred666@gmail.com>`
- License: `LGPL-3.0-or-later`
- Website: not set yet; future target is the GitHub repository URL.

### StatusNotifierItem Discovery

Implemented files:

- `src/statusnotifierscanner.h`
- `src/statusnotifierscanner.cpp`

Current behavior:

- Reads `org.kde.StatusNotifierWatcher.RegisteredStatusNotifierItems`.
- Reads `org.kde.StatusNotifierItem` properties through D-Bus.
- Captures `Id`, `Title`, `Category`, `Status`, `IconName`, `IconPixmap`, service, and path.
- Subscribes to item registration, unregistration, and icon/status update signals.
- Parses StatusNotifier pixmap payloads.
- Builds renderable `QIcon` objects from pixmap-only SNI data for restore.
- Generates data URLs for pixmap preview.
- Computes SHA1 hashes from pixmap width, height, and bytes.

Important verified cases:

- Yakuake exposes a named icon and restores through `IconName`.
- Telegram exposes pixmap data with an empty icon name and restores through a fresh `QIcon` built from `IconPixmap`.

### Plasma Internal Tray Discovery

Implemented in:

- `plasmoid/package/contents/ui/main.qml`

Current behavior:

- Finds the same-panel `org.kde.plasma.systemtray`.
- Locates its visual tray `GridView`.
- Extracts visual delegates for `StatusNotifier` and `Plasmoid` tray entries.
- Registers internal Plasma applet rows through `TrayItemModel::setPlasmaItems(...)`.
- Uses stable plugin ids for internal applets, such as `org.kde.plasma.clipboard`.

Limitation:

- Internal Plasma tray discovery currently requires KTrayMorph to be on the same panel as the system tray.
- Discovery is QML-tree based and therefore Plasma-version sensitive.

### Rule Engine

Implemented files:

- `src/ruleengine.h`
- `src/ruleengine.cpp`

Current behavior:

- Loads and saves rules from `~/.config/ktraymorph/rules.json`.
- Supports development override through `KTRAYMORPH_RULES_FILE`.
- Creates a replacement rule from a detected item.
- Replaces older matching rules for the same item when saving a new one.
- Applies exact-match rules from top to bottom.
- First matching rule wins.

Supported match types:

- `stableId`
- `sniId`
- `title`
- `iconName`
- `iconHash`
- `servicePath`

Current replacement values:

- Theme icon name string.
- Local user icon file path for SVG/PNG files.

Current replacement types:

- `themeIcon`
- `localFile`
- `colorTint`: recolor the current native icon through KTrayMorph's overlay instead of choosing a replacement icon.

Compatibility:

- Rules without `replacementType` are treated as `themeIcon`.

### Live Icon Replacement Lifecycle

Current intended lifecycle:

- Persistent rules live in `~/.config/ktraymorph/rules.json`.
- Rules survive logout, reboot, and shutdown.
- Live visual overrides exist only while the KTrayMorph widget is active.
- When a rule is saved, KTrayMorph immediately applies the live replacement to the current tray delegate.
- On startup, tray reload, or new SNI registration, persisted rules are re-applied to matching live tray delegates.
- When the widget is disabled, destroyed, or removed from the panel, `LiveReplacementController::restoreAll()` removes live overlays and clears only the live replacement records.
- Removing the widget does not delete `rules.json`.

Restore behavior:

- Restore removes KTrayMorph's overlay and reveals the native tray icon item.
- Internal Plasma applets keep their native icon binding alive underneath the overlay, so media play/pause/stop and volume level state restore to the current native state.
- Named SNI icons keep their native visual source underneath the overlay.
- Pixmap-only SNI icons keep their native visual pixmap underneath the overlay.

Guardrails:

- KTrayMorph does not write replacement values into the native icon's `source` property.
- Existing live overlays are re-targeted by stable id and reasserted even when the tray applet changes its state icon.
- Overlay items are destroyed on restore, retarget, target destruction, widget teardown, and rule deletion to avoid stale visual overlays.

### UI

Implemented file:

- `plasmoid/package/contents/ui/main.qml`

Current behavior:

- Shows detected SNI and internal Plasma tray items.
- On desktop placement, shows the normal UI with StatusNotifierItem rows only; Plasma internal tray items require same-panel system tray access.
- On panel placement without a same-panel Plasma system tray, shows only a placement warning in the expanded view.
- Shows original icon preview.
- Shows replacement preview when a rule exists.
- Shows title, item kind, stable id, status, pixmap hash prefix, and replacement icon name.
- Lets the user type a replacement theme icon name.
- Lets the user choose a custom local SVG/PNG icon file.
- Lets the user choose a tint color instead of a replacement icon.
- Color tint is limited to named/theme icons; pixmap-only SNI icons should use theme icon or local file replacement.
- Lets the user create/update a replacement rule per row.
- Lets the user undo one replacement rule per row.
- Lets the user clear all rules.
- Lets the user refresh discovery.
- Disables the main applet UI when replacements are globally inactive.
- Provides an applet context-menu action to enable or disable all replacements without opening the configuration page.
- Uses a desaturated applet icon when replacements are inactive.
- Uses tooltips for icon/action buttons.
- Uses wider default geometry to avoid a collapsed narrow popup after removing and re-adding the widget.

Icon picker:

- Opens from the header search button.
- Does not preselect a replacement icon on first launch.
- Searches installed theme icons incrementally.
- Runs icon search asynchronously through `QtConcurrent`.
- Debounces search input.
- Ignores stale async search results.
- Shows `Searching...` while a search is running.
- Shows icon previews in a grid.
- Shows full icon names in wrapped tooltips.
- Local SVG/PNG selection is a header action next to the icon search button, not part of the search popup.
- Shows only the local file name in the main field after selection, while storing the full path in the rule.
- Reopens/keeps the applet expanded after native file dialog selection.

### Configure UI

Implemented files:

- `plasmoid/package/contents/config/config.qml`
- `plasmoid/package/contents/config/main.xml`
- `plasmoid/package/contents/ui/configGeneral.qml`

Current behavior:

- Adds a standard Plasma `Configure KTrayMorph` page named `General`.
- Provides an `Enable logging` checkbox.
- Provides a log file path field.
- Provides a `Choose...` file picker button.
- Default log path is `/tmp/ktraymorph.log`.
- Provides an `Active` checkbox. When inactive, live overrides are restored, polling stops, and rules remain saved for later reactivation.
- Provides a polling interval field in milliseconds.
- Default polling interval is `850`.

### Logging

Implemented files:

- `src/logging.h`
- `src/logging.cpp`

Current behavior:

- Logging is disabled by default.
- When disabled, KTrayMorph does not write a log file.
- The previous hardcoded debug log path is removed.
- QML and C++ diagnostics go through the same gated logger.
- Former `console.log` and `qDebug` diagnostic noise has been replaced with managed logging.
- Runtime changes to `enableLogging` and `logFilePath` are applied immediately from `Plasmoid.configuration`.

### Diagnostic Tool

Implemented file:

- `src/dump_sni.cpp`

Purpose:

- Dumps current SNI model data to stdout.
- Useful for checking D-Bus access, icon names, pixmap hashes, and rule matching.
- Supports temporary rules through `KTRAYMORPH_RULES_FILE`.

Example:

```bash
build/bin/ktraymorph-dump-sni
```

Example with temporary rules:

```bash
KTRAYMORPH_RULES_FILE=/tmp/ktraymorph-rules.json build/bin/ktraymorph-dump-sni
```

## Verification Used

Build:

```bash
cmake --build build
```

Install:

```bash
sudo ./install.sh
```

Basic QML brace check:

```bash
awk '{ for (i=1;i<=length($0);i++) { c=substr($0,i,1); if (c=="{") b++; else if (c=="}") b--; if (b<0) { print "negative at", NR; exit 1 } } } END { print "brace balance", b; exit b != 0 }' plasmoid/package/contents/ui/main.qml
```

Metadata/config validation:

```bash
python -m json.tool plasmoid/package/metadata.json
python -c 'import xml.etree.ElementTree as ET; ET.parse("plasmoid/package/contents/config/main.xml"); print("xml ok")'
```

## Known Limitations

- Plasma internal tray replacement depends on same-panel QML tree inspection.
- There is no robust Plasma-version adapter layer yet.
- Rules are exact-match only.
- Rule editing is still minimal: create/update, per-row undo, clear all.
- Local user icon files have first-pass missing-file detection and UI marking.
- Missing local files remain as visible rules so the user can undo or replace them.
- Replacement values now support `themeIcon` and `localFile`, but there is no full editor for changing type on existing rules.
- Icon hash normalization is basic and uses largest pixmap data as-is.
- There is no automated test suite yet.
- QML validation is currently mostly build plus runtime loading; no full `qmllint` gate is wired into the workflow.

## Next Planned Work

Polish custom user icon support and rule management.

Initial target:

- Add a full rules editor for existing saved rules.
- Allow changing replacement type on an existing rule without recreating it from a row.
- Add a richer repair flow for missing local files.
- Add clearer validation messages when saving a new local-file rule fails.
- Keep existing theme-icon and local-file rules working across schema changes.

## Phase Status

- Phase 1: Project skeleton - complete for prototype.
- Phase 2: StatusNotifierItem discovery - implemented.
- Phase 3: Icon hashing - implemented at a basic level.
- Phase 4: Plasma tray item discovery - implemented through same-panel QML scan; robustness work remains.
- Phase 5: Rule engine - implemented for exact persistent rules; full editor remains.
- Phase 6: Icon replacement - working for tested Plasma applets, named SNI icons, and pixmap-only SNI icons.
- Phase 7: Native configuration UI - started; logging config and icon picker exist, rule editor remains.
- Phase 7.1: Custom user icons - first-pass local SVG/PNG support and missing-file UI implemented; rule editing polish remains.
- Phase 8: Performance pass - partially addressed for icon search with async/debounce; broader pass remains.
- Phase 9: Robustness - partially addressed with guarded live replacement and restore; adapter/tests remain.
- Phase 10: Packaging - local global install script works; uninstall/package docs remain.
