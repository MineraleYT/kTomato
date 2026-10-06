// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.ScrollablePage {
    id: page

    /// Empty means "create a new timer".
    property string uuid: ""

    readonly property bool isNew: uuid === ""
    readonly property var preset: isNew
        ? ({ name: "", category: "", iconName: "chronometer", workSeconds: 25 * 60, shortBreakSeconds: 5 * 60,
             longBreakSeconds: 15 * 60, cyclesBeforeLong: 4, builtin: false,
             options: ({ autoStartMode: 0, silenceWhileWorking: false, soundOnEnd: true, notifyOnEnd: true }) })
        : PresetModel.get(uuid)
    property string selectedIcon: isNew ? "chronometer" : (preset.iconName || "chronometer")
    readonly property var iconChoices: PresetModel.availableIcons()
    readonly property bool valid: nameField.text.trim().length > 0

    title: isNew ? i18n("New Timer") : i18n("Edit Timer")

    header: Kirigami.InlineMessage {
        visible: !SilenceController.available
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Warning
        text: i18n("This desktop's notification service cannot be silenced by applications, so “Silence notifications while working” has no effect here.")
    }

    // Spin box whose text carries a unit ("25 minutes"): any text is accepted while typing and
    // the number is read out of it; text without a number keeps the current value.
    component UnitSpinBox: QQC2.SpinBox {
        id: unitBox
        editable: true
        validator: RegularExpressionValidator { regularExpression: /.*/ }
        valueFromText: (text, locale) => {
            const match = text.match(/\d+/);
            return match ? parseInt(match[0]) : unitBox.value;
        }
    }

    // Titled spin box (label above the field).
    component MinutesField: ColumnLayout {
        id: minutesField
        required property int fieldFrom
        required property int fieldTo
        required property int fieldValue
        property string label
        property alias value: input.value
        spacing: Kirigami.Units.smallSpacing / 2

        QQC2.Label {
            text: minutesField.label
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        UnitSpinBox {
            id: input
            Layout.fillWidth: true
            from: minutesField.fieldFrom
            to: minutesField.fieldTo
            value: minutesField.fieldValue
            textFromValue: (value, locale) => i18ncp("@item:valuesuffix duration in a spin box",
                                                     "%1 minute", "%1 minutes", value)
            Accessible.name: minutesField.label
        }
    }

    // Flat Breeze-style section card.
    component Card: Rectangle {
        default property alias content: cardColumn.data
        property alias title: cardTitle.text
        Layout.fillWidth: true
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        radius: Kirigami.Units.cornerRadius
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
        implicitHeight: cardOuter.implicitHeight + Kirigami.Units.largeSpacing * 2

        ColumnLayout {
            id: cardOuter
            anchors {
                fill: parent
                margins: Kirigami.Units.largeSpacing
            }
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                id: cardTitle
                level: 4
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            ColumnLayout {
                id: cardColumn
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing
            }
        }
    }

    // Option row: title + one-line helper on the left, control on the right.
    component OptionRow: RowLayout {
        id: optionRow
        property string title
        property string helper
        default property alias control: controlHolder.data
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            QQC2.Label {
                Layout.fillWidth: true
                text: optionRow.title
                wrapMode: Text.WordWrap
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: optionRow.helper.length > 0
                text: optionRow.helper
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            id: controlHolder
        }
    }

    function save(): void {
        if (!valid) {
            return;
        }
        const fields = {
            name: nameField.text,
            category: categoryField.text,
            iconName: page.selectedIcon,
            workSeconds: workBox.value * 60,
            shortBreakSeconds: shortBox.value * 60,
            longBreakSeconds: longBox.value * 60,
            cyclesBeforeLong: cyclesBox.value,
            autoStartMode: autoStartBox.currentIndex,
            silenceWhileWorking: silenceBox.checked,
            soundOnEnd: soundBox.checked,
            notifyOnEnd: notifyBox.checked
        };
        if (isNew) {
            PresetModel.create(fields);
        } else {
            PresetModel.update(uuid, fields);
        }
        close();
    }

    readonly property string liveSummary: {
        const dur = (min) => i18ncp("duration in minutes", "%1 min", "%1 min", min);
        if (cyclesBox.value > 0 && longBox.value > 0) {
            return i18nc("%1 work duration, %2 break duration, %3 long break duration, %4 number of cycles",
                         "%1 work • %2 break • %3 long break (%4)",
                         dur(workBox.value), dur(shortBox.value), dur(longBox.value),
                         i18np("%1 cycle", "%1 cycles", cyclesBox.value));
        }
        return i18nc("%1 work duration, %2 break duration", "%1 work • %2 break",
                     dur(workBox.value), dur(shortBox.value));
    }

    function close(): void {
        applicationWindow().pageStack.pop();
    }

    Shortcut {
        sequences: [StandardKey.Save]
        enabled: page.isCurrentPage
        onActivated: page.save()
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: page.isCurrentPage && !autoStartBox.popup.visible
        onActivated: page.close()
    }

    // Sticky action bar.
    footer: Rectangle {
        implicitHeight: footerRow.implicitHeight + Kirigami.Units.largeSpacing * 2
        color: Kirigami.Theme.backgroundColor

        Kirigami.Separator {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
        }

        RowLayout {
            id: footerRow
            anchors {
                verticalCenter: parent.verticalCenter
                horizontalCenter: parent.horizontalCenter
            }
            width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 44)
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                visible: page.preset.builtin === true
                flat: true
                text: i18n("Reset to Defaults")
                icon.name: "edit-undo"
                onClicked: {
                    PresetModel.reset(page.uuid);
                    page.close();
                }
            }

            Item {
                Layout.fillWidth: true
            }

            QQC2.Button {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onClicked: page.close()
            }

            QQC2.Button {
                text: i18n("Save")
                icon.name: "document-save"
                highlighted: true
                enabled: page.valid
                onClicked: page.save()
            }
        }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: 0

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 44
            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            Card {
                title: i18n("Details")

                GridLayout {
                    Layout.fillWidth: true
                    columns: width > Kirigami.Units.gridUnit * 26 ? 2 : 1
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: Kirigami.Units.smallSpacing / 2
                        QQC2.Label {
                            text: i18n("Name:")
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                        QQC2.TextField {
                            id: nameField
                            Layout.fillWidth: true
                            text: page.preset.name
                            placeholderText: i18n("e.g. Deep work")
                            Accessible.name: i18n("Name:")
                            onAccepted: page.save()
                            Component.onCompleted: if (page.isNew) forceActiveFocus()
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: Kirigami.Units.smallSpacing / 2
                        QQC2.Label {
                            text: i18n("Category:")
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                        QQC2.TextField {
                            id: categoryField
                            Layout.fillWidth: true
                            text: page.preset.category
                            placeholderText: i18n("Optional, e.g. Work or Study")
                            Accessible.name: i18n("Category:")
                            onAccepted: page.save()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        text: i18n("Icon:")
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: page.iconChoices

                            delegate: QQC2.AbstractButton {
                                id: iconBtn
                                required property var modelData

                                readonly property bool isSelected: page.selectedIcon === modelData.iconName
                                hoverEnabled: true
                                // Not drawn (custom contentItem), but read by screen readers.
                                text: modelData.label
                                Accessible.role: Accessible.RadioButton
                                Accessible.checkable: true
                                Accessible.checked: isSelected
                                padding: Kirigami.Units.largeSpacing

                                implicitWidth: Kirigami.Units.gridUnit * 3
                                implicitHeight: Kirigami.Units.gridUnit * 3

                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: modelData.label
                                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                                background: Rectangle {
                                    radius: Kirigami.Units.cornerRadius
                                    color: iconBtn.isSelected
                                        ? Kirigami.Theme.highlightColor
                                        : (iconBtn.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.12) : Qt.alpha(Kirigami.Theme.textColor, 0.05))
                                    border.width: iconBtn.visualFocus ? 2 : 1
                                    border.color: iconBtn.isSelected || iconBtn.visualFocus
                                        ? Kirigami.Theme.highlightColor
                                        : Qt.alpha(Kirigami.Theme.textColor, iconBtn.hovered ? 0.3 : 0.15)

                                    Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
                                }

                                contentItem: Kirigami.Icon {
                                    source: iconBtn.modelData.iconName
                                    color: iconBtn.isSelected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                }

                                // Check mark on the current choice.
                                Rectangle {
                                    visible: iconBtn.isSelected
                                    width: Kirigami.Units.iconSizes.small
                                    height: width
                                    radius: width / 2
                                    anchors {
                                        top: parent.top
                                        right: parent.right
                                        margins: -Kirigami.Units.smallSpacing / 2
                                    }
                                    color: Kirigami.Theme.backgroundColor
                                    border.width: 1
                                    border.color: Kirigami.Theme.highlightColor

                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        width: parent.width - 4
                                        height: width
                                        source: "checkmark"
                                        color: Kirigami.Theme.highlightColor
                                    }
                                }

                                onClicked: page.selectedIcon = modelData.iconName
                            }
                        }
                    }
                }
            }

            Card {
                title: i18n("Durations")

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.largeSpacing

                    MinutesField {
                        id: workBox
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: i18n("Work:")
                        fieldFrom: 1
                        fieldTo: 360
                        fieldValue: Math.round(page.preset.workSeconds / 60)
                    }

                    MinutesField {
                        id: shortBox
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: i18n("Short break:")
                        fieldFrom: 0
                        fieldTo: 120
                        fieldValue: Math.round(page.preset.shortBreakSeconds / 60)
                    }

                    MinutesField {
                        id: longBox
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: i18n("Long break:")
                        fieldFrom: 0
                        fieldTo: 120
                        fieldValue: Math.round(page.preset.longBreakSeconds / 60)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: Kirigami.Units.smallSpacing / 2

                        QQC2.Label {
                            text: i18n("Long break after:")
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }

                        UnitSpinBox {
                            id: cyclesBox
                            Layout.fillWidth: true
                            Accessible.name: i18n("Long break after:")
                            from: 0
                            to: 99
                            value: page.preset.cyclesBeforeLong
                            textFromValue: (value, locale) => value === 0
                                ? i18n("Never")
                                : i18np("%1 work session", "%1 work sessions", value)
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: page.liveSummary
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: i18n("Options")

                OptionRow {
                    title: i18n("Auto-start mode")
                    helper: i18n("Choose whether breaks, work phases, or neither start automatically.")

                    QQC2.ComboBox {
                        id: autoStartBox
                        Accessible.name: i18n("Auto-start mode")
                        model: [i18n("Manual"), i18n("Breaks only"), i18n("All phases")]
                        currentIndex: (page.preset.options && page.preset.options.autoStartMode !== undefined)
                            ? Math.max(0, Math.min(2, Number(page.preset.options.autoStartMode)))
                            : 0
                    }
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                }

                OptionRow {
                    title: i18n("Silence notifications while working")
                    helper: i18n("Turns off all system notifications for the whole work phase and turns them back on during the break.")

                    QQC2.Switch {
                        id: silenceBox
                        Accessible.name: i18n("Silence notifications while working")
                        checked: (page.preset.options && page.preset.options.silenceWhileWorking !== undefined)
                            ? Boolean(page.preset.options.silenceWhileWorking)
                            : false
                        enabled: SilenceController.available
                    }
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                }

                OptionRow {
                    title: i18n("Play a sound when a phase ends")

                    QQC2.Switch {
                        id: soundBox
                        Accessible.name: i18n("Play a sound when a phase ends")
                        checked: (page.preset.options && page.preset.options.soundOnEnd !== undefined)
                            ? Boolean(page.preset.options.soundOnEnd)
                            : true
                    }
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                }

                OptionRow {
                    title: i18n("Show a notification when a phase ends")

                    QQC2.Switch {
                        id: notifyBox
                        Accessible.name: i18n("Show a notification when a phase ends")
                        checked: (page.preset.options && page.preset.options.notifyOnEnd !== undefined)
                            ? Boolean(page.preset.options.notifyOnEnd)
                            : true
                    }
                }
            }
        }
    }
}
