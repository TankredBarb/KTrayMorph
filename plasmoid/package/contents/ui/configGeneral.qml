import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: root

    property alias cfg_active: active.checked
    property alias cfg_enableLogging: enableLogging.checked
    property alias cfg_logFilePath: logFilePath.text
    property alias cfg_pollIntervalMs: pollIntervalMs.value

    Kirigami.FormLayout {
        anchors.fill: parent

        CheckBox {
            id: active

            Kirigami.FormData.label: i18n("Replacement:")
            text: i18n("Active")
        }

        CheckBox {
            id: enableLogging

            Kirigami.FormData.label: i18n("Logging:")
            text: i18n("Enable logging")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Log file:")
            enabled: enableLogging.checked
            Layout.fillWidth: true

            TextField {
                id: logFilePath

                Layout.fillWidth: true
                placeholderText: "/tmp/ktraymorph.log"
                selectByMouse: true
            }

            Button {
                icon.name: "document-open-symbolic"
                text: i18n("Choose...")
                onClicked: logFileDialog.open()
            }
        }

        SpinBox {
            id: pollIntervalMs

            Kirigami.FormData.label: i18n("Polling interval:")
            from: 250
            to: 5000
            stepSize: 50
            editable: true
            textFromValue: function(value, locale) {
                return i18nc("polling interval in milliseconds", "%1 ms", value);
            }
            valueFromText: function(text, locale) {
                const parsed = Number(String(text).replace(/[^\d]/g, ""));
                return Number.isFinite(parsed) ? parsed : 850;
            }
        }
    }

    FileDialog {
        id: logFileDialog

        title: i18n("Choose Log File")
        fileMode: FileDialog.SaveFile
        nameFilters: [i18n("Log files (*.log *.txt)"), i18n("All files (*)")]
        onAccepted: logFilePath.text = selectedFile.toString().replace(/^file:\/\//, "")
    }
}
