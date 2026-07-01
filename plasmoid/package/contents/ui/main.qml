pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

import org.ktraymorph.core

PlasmoidItem {
    id: root

    Plasmoid.icon: "org.ktraymorph.plasmoid"
    Plasmoid.status: PlasmaCore.Types.ActiveStatus
    readonly property bool onDesktop: Plasmoid.formFactor === PlasmaCore.Types.Planar
    preferredRepresentation: root.onDesktop ? fullRepresentation : null
    switchWidth: root.onDesktop ? -1 : Kirigami.Units.iconSizes.enormous
    switchHeight: root.onDesktop ? -1 : Kirigami.Units.iconSizes.enormous
    property string replacementIconName: ""
    property string replacementIconType: "themeIcon"
    property string plasmaTrayScanStatus: "Plasma tray scan has not run"
    property var plasmaTrayDelegatesById: ({})
    property bool iconPickerOpen: false
    property string iconSearchQuery: ""
    property var iconSearchResults: []
    property bool iconSearchPending: false
    property int iconSearchRequestId: 0
    property int startupScanAttempts: 0
    property int statusNotifierReapplyAttempts: 0
    property string plasmaTrayItemsSignature: ""
    property bool localIconDialogActive: false
    property bool trayPlacementChecked: false
    property bool validTrayPlacement: false
    readonly property int effectivePollIntervalMs: root.normalizedPollIntervalMs(Plasmoid.configuration.pollIntervalMs)

    onExpandedChanged: {
        if (root.expanded) {
            root.scanPlasmaTrayItems();
        } else if (localIconDialogActive) {
            Qt.callLater(function() {
                if (root.localIconDialogActive) {
                    root.expanded = true;
                }
            });
        }
    }

    TrayItemModel {
        id: trayItems

        onIconSearchFinished: function(requestId, filter, names) {
            if (requestId !== root.iconSearchRequestId || filter !== root.iconSearchQuery) {
                return;
            }
            root.iconSearchResults = names;
            root.iconSearchPending = false;
        }

        onStatusNotifierItemsReloaded: root.handleStatusNotifierItemsReloaded()

        onItemsReloaded: Qt.callLater(root.applyPersistedLiveReplacements)
    }

    LiveReplacementController {
        id: replacer
        trayModel: trayItems

        onNeedsRetarget: function(stableId) {
            root.debugLog("QML needsRetarget stableId=" + stableId);
            root.collectPlasmaTrayDelegates();
            root.applyPersistedLiveReplacements();
        }

        onRecordsChanged: {
            if (replacer.recordCount() > 0) {
                if (!overridePollTimer.running) {
                    overridePollTimer.restart();
                }
            }
        }
    }

    function configureLoggingFromSettings() {
        trayItems.configureLogging(Boolean(Plasmoid.configuration.enableLogging),
                                   String(Plasmoid.configuration.logFilePath ?? ""));
    }

    function debugLog(message) {
        trayItems.debugLog(message);
    }

    function normalizedPollIntervalMs(value) {
        const parsed = Number(value);
        if (!Number.isFinite(parsed)) {
            return 850;
        }
        return Math.max(250, Math.min(5000, Math.round(parsed)));
    }

    Connections {
        target: Plasmoid.configuration

        function onEnableLoggingChanged() {
            root.configureLoggingFromSettings();
        }

        function onLogFilePathChanged() {
            root.configureLoggingFromSettings();
        }

        function onPollIntervalMsChanged() {
            overridePollTimer.restart();
        }
    }

    function openIconPicker() {
        iconSearchQuery = replacementIconType === "themeIcon" ? replacementIconName : "";
        iconPickerOpen = true;
        updateIconSearch(iconSearchQuery);
        Qt.callLater(function() {
            iconSearchField.forceActiveFocus();
            iconSearchField.selectAll();
        });
    }

    function openLocalIconDialog() {
        localIconDialogActive = true;
        localIconDialog.open();
        Qt.callLater(function() {
            root.expanded = true;
        });
    }

    function updateIconSearch(query) {
        iconSearchQuery = query.trim();
        iconSearchDebounce.restart();
    }

    function runIconSearch() {
        if (iconSearchQuery.length === 0) {
            iconSearchResults = [];
            iconSearchPending = false;
            iconSearchRequestId = 0;
            return;
        }

        iconSearchPending = true;
        iconSearchRequestId = trayItems.searchIconNames(iconSearchQuery, 160);
    }

    function tooltipText(text) {
        const value = String(text);
        const maxLineLength = 34;
        let result = "";
        let lineLength = 0;

        for (let i = 0; i < value.length; i++) {
            const ch = value[i];
            if (lineLength >= maxLineLength && (ch === "-" || ch === "_" || ch === ".")) {
                result += ch + "\n";
                lineLength = 0;
                continue;
            }
            if (lineLength >= maxLineLength) {
                result += "\n";
                lineLength = 0;
            }
            result += ch;
            lineLength += 1;
        }

        return result;
    }

    function localFileSource(path) {
        if (path.indexOf("file://") === 0) {
            return path;
        }
        return "file://" + encodeURI(path);
    }

    function replacementSource(type, value) {
        if (type === "localFile") {
            return localFileSource(value);
        }
        return value;
    }

    function currentReplacementSource() {
        if (replacementIconName.length === 0) {
            return "image-missing-symbolic";
        }
        return replacementSource(replacementIconType, replacementIconName);
    }

    function selectedFilePath(url) {
        return decodeURIComponent(String(url).replace(/^file:\/\//, ""));
    }

    function fileName(path) {
        const parts = String(path).split("/");
        return parts.length > 0 ? parts[parts.length - 1] : String(path);
    }

    FileDialog {
        id: localIconDialog

        title: "Choose custom icon"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Icon files (*.svg *.png)", "All files (*)"]
        onAccepted: {
            root.replacementIconType = "localFile";
            root.replacementIconName = root.selectedFilePath(selectedFile);
            root.iconPickerOpen = false;
            Qt.callLater(function() {
                root.expanded = true;
                root.localIconDialogActive = false;
            });
        }
        onRejected: {
            Qt.callLater(function() {
                root.expanded = true;
                root.localIconDialogActive = false;
            });
        }
    }

    Timer {
        id: iconSearchDebounce
        interval: 140
        repeat: false
        onTriggered: root.runIconSearch()
    }

    Timer {
        id: startupScanRetry
        interval: 500
        repeat: false
        onTriggered: root.scanPlasmaTrayItems(true)
    }

    Timer {
        id: statusNotifierReapplyTimer
        interval: 80
        repeat: false
        onTriggered: root.runStatusNotifierReapplyPass()
    }

    Timer {
        id: overridePollTimer
        interval: root.effectivePollIntervalMs
        repeat: true
        running: true
        onTriggered: {
            if (replacer.recordCount() === 0 && !trayItems.hasReplacementItems()) {
                return;
            }
            const replacementItems = trayItems.replacementItems();

            const result = root.collectPlasmaTrayDelegates();
            root.trayPlacementChecked = true;
            root.validTrayPlacement = result.ok;
            root.plasmaTrayScanStatus = result.status;
            if (!result.ok) {
                return;
            }
            root.updatePlasmaTrayItems(result.items, false);
            root.applyPersistedLiveReplacements(replacementItems);
            replacer.reassertAll();
        }
    }

    function findPanelLayout() {
        let candidate = root.parent;
        while (candidate) {
            if (candidate instanceof GridLayout) {
                return candidate;
            }
            candidate = candidate.parent;
        }
        return null;
    }

    function findTrayGridView(item) {
        if (!item || !item.children) {
            return null;
        }
        if (item instanceof GridView) {
            return item;
        }
        for (let i = 0; i < item.children.length; i++) {
            const result = findTrayGridView(item.children[i]);
            if (result) {
                return result;
            }
        }
        return null;
    }

    function findSystemTrayItem(panelLayout) {
        if (!panelLayout || !panelLayout.children) {
            return null;
        }
        for (let i = 0; i < panelLayout.children.length; i++) {
            const child = panelLayout.children[i];
            const pluginName = child?.applet?.plasmoid?.pluginName ?? "";
            if (pluginName === "org.kde.plasma.systemtray") {
                return child;
            }
        }
        return null;
    }

    function scheduleStartupScanRetry() {
        if (startupScanAttempts >= 20) {
            return;
        }

        startupScanAttempts += 1;
        startupScanRetry.restart();
    }

    function handleStatusNotifierItemsReloaded() {
        root.debugLog("QML SNI reload received; scheduling tray scan and delayed replacement reapply");
        Qt.callLater(root.scanPlasmaTrayItems);
        scheduleStatusNotifierReapply();
    }

    function scheduleStatusNotifierReapply() {
        statusNotifierReapplyAttempts = 0;
        statusNotifierReapplyTimer.interval = 80;
        statusNotifierReapplyTimer.restart();
    }

    function runStatusNotifierReapplyPass() {
        statusNotifierReapplyAttempts += 1;
        root.debugLog("QML SNI delayed reapply pass=" + statusNotifierReapplyAttempts
            + " liveCount=" + replacer.recordCount());

        root.scanPlasmaTrayItems();
        root.applyPersistedLiveReplacements();

        if (statusNotifierReapplyAttempts >= 4) {
            return;
        }

        statusNotifierReapplyTimer.interval = statusNotifierReapplyAttempts === 1 ? 160
            : statusNotifierReapplyAttempts === 2 ? 320
            : 640;
        statusNotifierReapplyTimer.restart();
    }

    function collectPlasmaTrayDelegates() {
        plasmaTrayDelegatesById = ({});
        const items = [];
        let statusNotifierVisualCount = 0;

        const panelLayout = findPanelLayout();
        if (!panelLayout) {
            return { "ok": false, "status": "Plasma tray: not on a panel", "items": items, "sni": 0 };
        }

        const systemTray = findSystemTrayItem(panelLayout);
        if (!systemTray) {
            return { "ok": false, "status": "Plasma tray: system tray not found on this panel", "items": items, "sni": 0 };
        }

        const trayGridView = findTrayGridView(systemTray);
        if (!trayGridView) {
            return { "ok": false, "status": "Plasma tray: grid view not found", "items": items, "sni": 0 };
        }

        const seen = new Set();
        for (let i = 0; i < trayGridView.count; i++) {
            const delegateItem = trayGridView.itemAtIndex(i);
            if (!delegateItem || !delegateItem.children) {
                continue;
            }

            for (let j = 0; j < delegateItem.children.length; j++) {
                const model = delegateItem.children[j]?.model;
                if (!model) {
                    continue;
                }

                if (model.itemType === "StatusNotifier") {
                    const stableId = model.Id ?? "";
                    if (stableId.length === 0 || seen.has(stableId)) {
                        continue;
                    }

                    seen.add(stableId);
                    statusNotifierVisualCount += 1;
                    plasmaTrayDelegatesById[stableId] = {
                        "delegateItem": delegateItem,
                        "modelItem": delegateItem.children[j],
                        "itemType": "StatusNotifier"
                    };
                    continue;
                }

                if (model.itemType !== "Plasmoid") {
                    continue;
                }

                const applet = model.applet ?? null;
                const stableId = applet?.plasmoid?.pluginName ?? "";
                if (stableId.length === 0 || seen.has(stableId)) {
                    continue;
                }

                seen.add(stableId);
                plasmaTrayDelegatesById[stableId] = {
                    "delegateItem": delegateItem,
                    "modelItem": delegateItem.children[j],
                    "applet": applet,
                    "itemType": "Plasmoid"
                };
                items.push({
                    "stableId": stableId,
                    "title": applet?.plasmoid?.title ?? stableId,
                    "iconName": applet?.plasmoid?.icon ?? "",
                    "status": "Active"
                });
            }
        }

        return { "ok": true, "status": "Plasma tray: " + items.length + " internal, "
            + statusNotifierVisualCount + " SNI visual item(s)", "items": items, "sni": statusNotifierVisualCount };
    }

    function trayItemsSignature(items) {
        const ids = [];
        for (let i = 0; i < items.length; i++) {
            ids.push(String(items[i].stableId ?? ""));
        }
        ids.sort();
        return ids.join("|");
    }

    function updatePlasmaTrayItems(items, force) {
        const signature = root.trayItemsSignature(items);
        if (!force && signature === plasmaTrayItemsSignature) {
            return false;
        }

        plasmaTrayItemsSignature = signature;
        trayItems.setPlasmaItems(items);
        return true;
    }

    function scanPlasmaTrayItems(retryOnMissing) {
        const result = collectPlasmaTrayDelegates();
        plasmaTrayScanStatus = result.status;
        trayPlacementChecked = true;
        validTrayPlacement = result.ok;

        if (!result.ok) {
            plasmaTrayItemsSignature = "";
            trayItems.setPlasmaItems([]);
            if (retryOnMissing) {
                scheduleStartupScanRetry();
            }
            return;
        }

        root.updatePlasmaTrayItems(result.items, true);
        root.debugLog("QML scanPlasmaTrayItems completed. Status=" + plasmaTrayScanStatus
            + " Keys in delegatesById=[" + Object.keys(plasmaTrayDelegatesById).join(",") + "]");
        if (retryOnMissing && Object.keys(plasmaTrayDelegatesById).length === 0) {
            scheduleStartupScanRetry();
        }
        Qt.callLater(root.applyPersistedLiveReplacements);
    }

    function hasProperty(item, propertyName) {
        return item !== null && item !== undefined && propertyName in item;
    }

    function sourceText(source) {
        return String(source ?? "");
    }

    function sourceHasValue(source) {
        return source !== null && source !== undefined && sourceText(source).length > 0;
    }

    function findIconSourceTarget(item, expectedSource, depth) {
        if (!item || depth > 6) {
            return null;
        }

        if (hasProperty(item, "source")) {
            const currentSourceText = sourceText(item.source);
            if (currentSourceText.length > 0 && currentSourceText === expectedSource) {
                return item;
            }
        }

        if (!item.children) {
            return null;
        }

        for (let i = 0; i < item.children.length; i++) {
            const result = findIconSourceTarget(item.children[i], expectedSource, depth + 1);
            if (result) {
                return result;
            }
        }

        return null;
    }

    function directIconSourceTarget(bundle) {
        if (!bundle) {
            return null;
        }

        const containers = [];
        if (bundle?.modelItem?.item?.iconContainer) {
            containers.push(bundle.modelItem.item.iconContainer);
        }
        if (bundle?.modelItem?.iconContainer) {
            containers.push(bundle.modelItem.iconContainer);
        }

        for (let i = 0; i < containers.length; i++) {
            const container = containers[i];
            if (!container || !container.children) {
                continue;
            }
            for (let j = 0; j < container.children.length; j++) {
                const child = container.children[j];
                if (hasProperty(child, "source")) {
                    return child;
                }
            }
        }

        return null;
    }

    function iconTargetForStableId(stableId, expectedSource) {
        const bundle = plasmaTrayDelegatesById[stableId] ?? null;
        if (!bundle) {
            return null;
        }

        const directTarget = directIconSourceTarget(bundle);
        if (directTarget) {
            return directTarget;
        }

        if (expectedSource.length === 0) {
            return null;
        }

        let target = findIconSourceTarget(bundle.modelItem ?? null, expectedSource, 0);
        if (target) {
            return target;
        }

        target = findIconSourceTarget(bundle.delegateItem ?? null, expectedSource, 0);
        if (target) {
            return target;
        }

        return findIconSourceTarget(bundle.applet?.compactRepresentationItem ?? null, expectedSource, 0);
    }

    function applyLiveReplacement(stableId, originalIcon, service, path, replacementType, replacementIcon) {
        const normalizedReplacement = replacementIcon.trim();
        if (stableId.length === 0 || normalizedReplacement.length === 0) {
            root.debugLog("QML applyLiveReplacement refused: empty stableId or replacementIcon");
            return false;
        }

        const replacementSourceValue = replacementSource(replacementType, normalizedReplacement);
        const replacementSourceText = sourceText(replacementSourceValue);

        if (replacer.hasRecord(stableId)) {
            const retarget = iconTargetForStableId(stableId, originalIcon);
            if (retarget) {
                replacer.retarget(stableId, retarget);
            }
            root.debugLog("QML applyLiveReplacement updating existing record stableId=" + stableId
                + " replacementSource=" + replacementSourceText);
            return replacer.updateReplacement(stableId, replacementType, normalizedReplacement, replacementSourceText);
        }

        const bundle = plasmaTrayDelegatesById[stableId] ?? null;
        const isStatusNotifier = bundle?.itemType === "StatusNotifier";

        if (!isStatusNotifier && originalIcon.length === 0) {
            root.debugLog("QML applyLiveReplacement refused: no original icon for plasmoid stableId=" + stableId);
            return false;
        }

        const target = iconTargetForStableId(stableId, originalIcon);
        if (!target) {
            plasmaTrayScanStatus = "Plasma tray: icon target not found for " + stableId;
            root.debugLog("QML applyLiveReplacement: target icon not found in QML tree for stableId=" + stableId);
            return false;
        }

        return replacer.registerReplacement(stableId,
                                             target,
                                             originalIcon,
                                             replacementSourceText,
                                             replacementType,
                                             normalizedReplacement,
                                             originalIcon,
                                             isStatusNotifier,
                                             service ?? "",
                                             path ?? "");
    }

    function dumpQmlTree(item, depth) {
        if (!item) {
            return "null";
        }
        let indent = "";
        for (let i = 0; i < depth; i++) {
            indent += "  ";
        }
        let result = indent + item.toString();
        let extra = [];
        if (hasProperty(item, "objectName") && item.objectName.length > 0) extra.push("objectName: " + item.objectName);
        if (hasProperty(item, "source")) extra.push("source: " + item.source);
        if (hasProperty(item, "iconName")) extra.push("iconName: " + item.iconName);
        if (hasProperty(item, "icon")) extra.push("icon: " + item.icon);
        if (hasProperty(item, "usesPlasmaTheme")) extra.push("usesPlasmaTheme: " + item.usesPlasmaTheme);
        if (extra.length > 0) {
            result += " (" + extra.join(", ") + ")";
        }
        result += "\n";

        if (item.children) {
            for (let i = 0; i < item.children.length; i++) {
                result += dumpQmlTree(item.children[i], depth + 1);
            }
        }
        return result;
    }

    function applyPersistedLiveReplacement(stableId, originalIcon, service, path, replacementType, replacementIcon, replacementAvailable) {
        if (!replacementAvailable || !replacementIcon || replacementIcon.length === 0) {
            return false;
        }

        return applyLiveReplacement(stableId, originalIcon, service, path, replacementType, replacementIcon);
    }

    function applyPersistedLiveReplacements(items) {
        const replacementItems = items ?? trayItems.replacementItems();
        for (let i = 0; i < replacementItems.length; i++) {
            applyPersistedLiveReplacement(replacementItems[i].stableId,
                                          replacementItems[i].iconName,
                                          replacementItems[i].service,
                                          replacementItems[i].path,
                                          replacementItems[i].replacementType,
                                          replacementItems[i].replacementIcon,
                                          replacementItems[i].replacementAvailable);
        }
    }

    Component.onCompleted: {
        root.configureLoggingFromSettings();
        root.debugLog("=== applet QML loaded ===");
        Qt.callLater(function() {
            root.scanPlasmaTrayItems(true);
        });
    }
    Component.onDestruction: {
        overridePollTimer.stop();
        replacer.restoreAll();
    }

    fullRepresentation: PlasmaExtras.Representation {
        implicitWidth: Kirigami.Units.gridUnit * 38
        implicitHeight: Kirigami.Units.gridUnit * 24
        Layout.minimumWidth: Kirigami.Units.gridUnit * 30
        Layout.minimumHeight: Kirigami.Units.gridUnit * 18
        Layout.preferredWidth: Kirigami.Units.gridUnit * 38
        Layout.preferredHeight: Kirigami.Units.gridUnit * 24
        collapseMarginsHint: true

        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                visible: root.onDesktop || (root.trayPlacementChecked && root.validTrayPlacement)
                spacing: 0

            PlasmaExtras.PlasmoidHeading {
                Layout.fillWidth: true

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: root.currentReplacementSource()
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    }

                    PlasmaComponents3.TextField {
                        id: replacementIconField

                        text: root.replacementIconType === "localFile"
                            ? root.fileName(root.replacementIconName)
                            : root.replacementIconName
                        placeholderText: "Icon name or custom file"
                        selectByMouse: true
                        Layout.fillWidth: true
                        onTextEdited: {
                            root.replacementIconType = "themeIcon";
                            root.replacementIconName = text.trim();
                        }
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: "edit-find-symbolic"
                        PlasmaComponents3.ToolTip.text: "Choose replacement icon"
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: root.openIconPicker()
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: "document-open-symbolic"
                        PlasmaComponents3.ToolTip.text: "Choose custom SVG or PNG icon"
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: root.openLocalIconDialog()
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: "view-refresh-symbolic"
                        PlasmaComponents3.ToolTip.text: "Refresh tray items"
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: {
                            replacer.restoreAll();
                            trayItems.refresh();
                            root.scanPlasmaTrayItems();
                        }
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: "edit-clear-symbolic"
                        PlasmaComponents3.ToolTip.text: "Clear all replacement rules"
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: {
                            replacer.restoreAll();
                            trayItems.clearRules();
                        }
                    }
                }
            }

            ListView {
                id: listView

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: trayItems
                spacing: Kirigami.Units.smallSpacing

                section.property: "kind"
                section.delegate: PlasmaExtras.PlasmoidHeading {
                    id: sectionHeading

                    required property string section
                    width: listView.width
                    contentItem: PlasmaComponents3.Label {
                        text: sectionHeading.section + " | " + root.plasmaTrayScanStatus
                        elide: Text.ElideRight
                        opacity: 0.8
                    }
                }

                delegate: PlasmaComponents3.ItemDelegate {
                    id: itemDelegate

                    required property string stableId
                    required property string kind
                    required property string title
                    required property string iconName
                    required property string status
                    required property string iconHash
                    required property bool hasPixmapIcon
                    required property string pixmapDataUrl
                    required property string replacementIcon
                    required property string replacementType
                    required property bool replacementAvailable
                    required property string replacementStatus
                    required property bool hasReplacement
                    required property string service
                    required property string path
                    required property int index

                    width: ListView.view.width
                    text: title
                    icon.name: iconName.length > 0 ? iconName : "image-missing-symbolic"

                    Component.onCompleted: {
                        root.debugLog("QML Item: " + title + " iconName=" + iconName
                            + " hasPixmapIcon=" + hasPixmapIcon + " pixmapDataUrl len=" + pixmapDataUrl.length);
                        if (hasReplacement) {
                            Qt.callLater(function() {
                                root.applyPersistedLiveReplacement(itemDelegate.stableId,
                                                                   itemDelegate.iconName,
                                                                   itemDelegate.service,
                                                                   itemDelegate.path,
                                                                   itemDelegate.replacementType,
                                                                   itemDelegate.replacementIcon,
                                                                   itemDelegate.replacementAvailable);
                            });
                        }
                    }

                    onReplacementIconChanged: {
                        if (hasReplacement) {
                            Qt.callLater(function() {
                                root.applyPersistedLiveReplacement(itemDelegate.stableId,
                                                                   itemDelegate.iconName,
                                                                   itemDelegate.service,
                                                                   itemDelegate.path,
                                                                   itemDelegate.replacementType,
                                                                   itemDelegate.replacementIcon,
                                                                   itemDelegate.replacementAvailable);
                            });
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        visible: !itemDelegate.hasPixmapIcon
                        source: itemDelegate.iconName.length > 0 ? itemDelegate.iconName : "image-missing-symbolic"
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    }

                    Image {
                        visible: itemDelegate.hasPixmapIcon
                        source: itemDelegate.pixmapDataUrl
                        width: Kirigami.Units.iconSizes.smallMedium
                        height: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        fillMode: Image.PreserveAspectFit
                    }

                    Kirigami.Icon {
                        source: "go-next-symbolic"
                        visible: itemDelegate.hasReplacement
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        opacity: 0.65
                    }

                    Kirigami.Icon {
                        source: itemDelegate.hasReplacement && itemDelegate.replacementIcon.length > 0
                            ? itemDelegate.replacementAvailable
                                ? root.replacementSource(itemDelegate.replacementType, itemDelegate.replacementIcon)
                                : "dialog-warning-symbolic"
                            : "image-missing-symbolic"
                        visible: itemDelegate.hasReplacement
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                        PlasmaComponents3.ToolTip.text: itemDelegate.replacementAvailable
                            ? root.tooltipText(itemDelegate.replacementIcon)
                            : itemDelegate.replacementStatus + ": " + root.tooltipText(itemDelegate.replacementIcon)
                        PlasmaComponents3.ToolTip.visible: itemDelegate.hasReplacement && itemDelegate.hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        PlasmaComponents3.Label {
                            text: itemDelegate.title
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        PlasmaComponents3.Label {
                            text: itemDelegate.kind + " | " + itemDelegate.stableId + " | " + itemDelegate.status
                                + (itemDelegate.hasPixmapIcon ? " | " + itemDelegate.iconHash.substring(0, 12) : "")
                                + (itemDelegate.hasReplacement
                                    ? " | -> " + itemDelegate.replacementType + ": " + itemDelegate.replacementIcon
                                        + (itemDelegate.replacementAvailable ? "" : " | " + itemDelegate.replacementStatus)
                                    : "")
                            elide: Text.ElideRight
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                            Layout.fillWidth: true
                        }
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: itemDelegate.hasReplacement ? "ktraymorph-update-symbolic" : "document-replace-symbolic"
                        enabled: root.replacementIconName.length > 0
                        PlasmaComponents3.ToolTip.text: itemDelegate.hasReplacement
                            ? "Update replacement for " + itemDelegate.title
                            : "Replace " + itemDelegate.title + " with selected icon"
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: {
                            root.debugLog("QML Click replace for stableId=" + itemDelegate.stableId
                                + " index=" + itemDelegate.index + " replacementIconName=" + root.replacementIconName);
                            const added = trayItems.addReplacementRule(itemDelegate.index,
                                                                       root.replacementIconType,
                                                                       root.replacementIconName);
                            root.debugLog("QML addReplacementRule returned=" + added);
                            if (added && (itemDelegate.kind === "PlasmaApplet" || itemDelegate.kind === "StatusNotifier")) {
                                const applied = root.applyLiveReplacement(itemDelegate.stableId,
                                                                         itemDelegate.iconName,
                                                                         itemDelegate.service,
                                                                         itemDelegate.path,
                                                                         root.replacementIconType,
                                                                         root.replacementIconName);
                                root.debugLog("QML applyLiveReplacement returned=" + applied);
                            }
                        }
                    }

                    PlasmaComponents3.ToolButton {
                        icon.name: "edit-undo-symbolic"
                        visible: itemDelegate.hasReplacement
                        PlasmaComponents3.ToolTip.text: "Restore original icon for " + itemDelegate.title
                        PlasmaComponents3.ToolTip.visible: hovered
                        PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: {
                            replacer.restore(itemDelegate.stableId);
                            trayItems.removeReplacementRules(itemDelegate.index);
                        }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: root.iconPickerOpen
                z: 10
                color: Qt.rgba(0, 0, 0, 0.35)

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.iconPickerOpen = false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 34)
                    height: Math.min(parent.height - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 22)
                    radius: Kirigami.Units.smallSpacing
                    color: Kirigami.Theme.backgroundColor
                    border.color: Kirigami.Theme.disabledTextColor
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: root.currentReplacementSource()
                                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                            }

                            PlasmaComponents3.TextField {
                                id: iconSearchField

                                text: root.iconSearchQuery
                                placeholderText: "Search icons"
                                selectByMouse: true
                                Layout.fillWidth: true
                                onTextEdited: root.updateIconSearch(text)
                                Keys.onEscapePressed: root.iconPickerOpen = false
                            }

                            PlasmaComponents3.ToolButton {
                                icon.name: "dialog-close-symbolic"
                                PlasmaComponents3.ToolTip.text: "Close icon picker"
                                PlasmaComponents3.ToolTip.visible: hovered
                                PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
                                onClicked: root.iconPickerOpen = false
                            }
                        }

                        PlasmaComponents3.Label {
                            text: root.iconSearchQuery.length === 0
                                ? "Type to search"
                                : root.iconSearchPending
                                    ? "Searching..."
                                : root.iconSearchResults.length + " icons"
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        GridView {
                            id: iconGrid

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.iconSearchResults
                            cellWidth: Kirigami.Units.gridUnit * 5
                            cellHeight: Kirigami.Units.gridUnit * 4

                            delegate: PlasmaComponents3.ItemDelegate {
                                required property string modelData

                                width: iconGrid.cellWidth
                                height: iconGrid.cellHeight
                                PlasmaComponents3.ToolTip.text: root.tooltipText(modelData)
                                PlasmaComponents3.ToolTip.visible: hovered
                                PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay

                                onClicked: {
                                    root.replacementIconType = "themeIcon";
                                    root.replacementIconName = modelData;
                                    root.iconPickerOpen = false;
                                }

                                contentItem: ColumnLayout {
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Icon {
                                        source: modelData
                                        implicitWidth: Kirigami.Units.iconSizes.medium
                                        implicitHeight: Kirigami.Units.iconSizes.medium
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    PlasmaComponents3.Label {
                                        text: modelData
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignHCenter
                                        font: Kirigami.Theme.smallFont
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PlasmaComponents3.Label {
                anchors.centerIn: parent
                width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 24)
                visible: !root.onDesktop && root.trayPlacementChecked && !root.validTrayPlacement
                text: "KTrayMorph works only on the same panel as the Plasma System Tray."
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
}
}
