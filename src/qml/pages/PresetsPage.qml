// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.ScrollablePage {
    id: page

    title: i18n("Timers")

    actions: [
        Kirigami.Action {
            icon.name: "list-add"
            text: i18n("New Timer")
            onTriggered: page.openEditor("")
        },
        Kirigami.Action {
            icon.name: "bookmark-new"
            text: i18n("Add Starter Presets")
            tooltip: i18n("Add standard presets (Pomodoro, Deep Work, Study, Quick Sprint)")
            onTriggered: {
                const before = PresetModel.count;
                PresetModel.addStandardPresets();
                if (PresetModel.count === before) {
                    applicationWindow().showPassiveNotification(i18n("All starter timers are already present."));
                }
            }
        }
    ]

    // Selecting another timer is refused while one is running: say so instead of silently
    // ignoring clicks.
    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        visible: TimerEngine.active
        type: Kirigami.MessageType.Information
        text: i18n("A timer is running. Stop it to switch to a different timer.")
    }

    function openEditor(uuid: string): void {
        applicationWindow().pushPage("PresetEditPage.qml", { uuid: uuid });
    }

    // Flat icon button, subdued until hovered/focused; `danger` turns it red on hover.
    component RowButton: QQC2.ToolButton {
        id: rowButton
        property bool danger: false
        readonly property bool emphasised: hovered || visualFocus
        display: QQC2.AbstractButton.IconOnly
        padding: Kirigami.Units.smallSpacing * 1.5
        // Hover tint: negative colour for the destructive button, neutral otherwise.
        background: Rectangle {
            radius: Kirigami.Units.cornerRadius
            color: rowButton.enabled && rowButton.emphasised
                   ? Qt.alpha(rowButton.danger ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor,
                              rowButton.down ? 0.25 : 0.12)
                   : "transparent"
            Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
        }
        contentItem: Kirigami.Icon {
            source: rowButton.icon.name
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
            opacity: !rowButton.enabled ? 0.4 : (rowButton.emphasised ? 1.0 : 0.65)
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
        }
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        Accessible.name: text
    }

    Kirigami.PromptDialog {
        id: deleteDialog

        property string uuid
        property string presetName

        title: i18n("Delete Timer")
        subtitle: i18n("Delete “%1”? Statistics already recorded with it are kept.", presetName)
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    PresetModel.remove(deleteDialog.uuid);
                    deleteDialog.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: deleteDialog.close()
            }
        ]
    }

    ListView {
        id: list

        model: PresetModel
        currentIndex: PresetModel.currentIndex
        spacing: Kirigami.Units.largeSpacing
        topMargin: Kirigami.Units.largeSpacing
        bottomMargin: Kirigami.Units.largeSpacing
        clip: true

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "chronometer"
            text: i18n("Timers")
            explanation: i18n("Add standard presets (Pomodoro, Deep Work, Study, Quick Sprint)")
            helpfulAction: Kirigami.Action {
                icon.name: "bookmark-new"
                text: i18n("Add Starter Presets")
                onTriggered: PresetModel.addStandardPresets()
            }
        }

        delegate: Item {
            id: delegate

            required property int index
            required property string uuid
            required property string name
            required property string category
            required property string summary
            required property string detailedSummary
            required property string iconName
            required property bool builtin

            readonly property bool isCurrent: PresetModel.currentUuid === uuid
            readonly property bool hovered: cardArea.hovered || rowHover.hovered

            width: ListView.view.width
            implicitHeight: card.implicitHeight
            height: implicitHeight

            Accessible.role: Accessible.Button
            Accessible.name: name
            Accessible.onPressAction: cardArea.clicked()

            QQC2.ItemDelegate {
                id: cardArea
                anchors.fill: card
                // Not drawn (custom contentItem), but read by screen readers.
                text: delegate.name
                background: null
                contentItem: Item {}
                onClicked: {
                    // The selection is locked while a phase is running or paused.
                    if (!TimerEngine.active) {
                        PresetModel.currentUuid = delegate.uuid;
                    }
                }
            }

            Rectangle {
                id: card
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 44)
                implicitHeight: cardLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: Kirigami.Units.cornerRadius
                color: delegate.isCurrent
                       ? Qt.alpha(Kirigami.Theme.highlightColor, 0.1)
                       : (cardArea.pressed ? Qt.alpha(Kirigami.Theme.textColor, 0.08)
                          : (delegate.hovered ? Kirigami.Theme.alternateBackgroundColor
                                              : Kirigami.Theme.backgroundColor))
                border.width: delegate.isCurrent ? 2 : 1
                border.color: delegate.isCurrent ? Kirigami.Theme.highlightColor
                              : Qt.alpha(Kirigami.Theme.textColor, delegate.hovered ? 0.3 : 0.15)

                Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
                Behavior on border.color { ColorAnimation { duration: Kirigami.Units.shortDuration } }

                HoverHandler {
                    id: rowHover
                }

                RowLayout {
                    id: cardLayout
                    anchors {
                        fill: parent
                        margins: Kirigami.Units.largeSpacing
                    }
                    spacing: Kirigami.Units.largeSpacing

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Kirigami.Units.iconSizes.large + Kirigami.Units.largeSpacing
                        Layout.preferredHeight: Layout.preferredWidth
                        radius: width / 2
                        color: Qt.alpha(Kirigami.Theme.highlightColor, delegate.isCurrent ? 0.25 : 0.12)

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            source: delegate.iconName
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            color: delegate.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                level: 3
                                text: delegate.name
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                visible: delegate.category.length > 0
                                radius: height / 2
                                color: Qt.alpha(Kirigami.Theme.textColor, 0.1)
                                implicitWidth: catLabel.implicitWidth + Kirigami.Units.largeSpacing
                                implicitHeight: catLabel.implicitHeight + Kirigami.Units.smallSpacing
                                Layout.preferredWidth: implicitWidth
                                Layout.preferredHeight: implicitHeight

                                QQC2.Label {
                                    id: catLabel
                                    anchors.centerIn: parent
                                    text: delegate.category
                                    font: Kirigami.Theme.smallFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }

                            Rectangle {
                                visible: delegate.isCurrent
                                radius: height / 2
                                color: Kirigami.Theme.highlightColor
                                implicitWidth: activeRow.implicitWidth + Kirigami.Units.largeSpacing
                                implicitHeight: activeRow.implicitHeight + Kirigami.Units.smallSpacing
                                Layout.preferredWidth: implicitWidth
                                Layout.preferredHeight: implicitHeight

                                Row {
                                    id: activeRow
                                    anchors.centerIn: parent
                                    spacing: Kirigami.Units.smallSpacing / 2
                                    Kirigami.Icon {
                                        source: "checkmark"
                                        width: Kirigami.Units.iconSizes.small
                                        height: width
                                        color: Kirigami.Theme.highlightedTextColor
                                    }
                                    QQC2.Label {
                                        text: i18nc("@label badge on the selected timer", "Active")
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.highlightedTextColor
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: delegate.detailedSummary.length > 0 ? delegate.detailedSummary : delegate.summary
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 0

                        RowButton {
                            icon.name: "document-edit"
                            text: i18n("Edit")
                            onClicked: page.openEditor(delegate.uuid)
                        }
                        RowButton {
                            icon.name: "edit-copy"
                            text: i18n("Duplicate")
                            onClicked: PresetModel.duplicate(delegate.uuid)
                        }
                        // A disabled button gets no hover events, so the wrapper shows why the
                        // default timer cannot be deleted.
                        Item {
                            implicitWidth: deleteButton.implicitWidth
                            implicitHeight: deleteButton.implicitHeight

                            HoverHandler {
                                id: deleteHover
                            }

                            QQC2.ToolTip.visible: delegate.builtin && deleteHover.hovered
                            QQC2.ToolTip.text: deleteButton.text
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                            RowButton {
                                id: deleteButton
                                anchors.fill: parent
                                danger: true
                                icon.name: "edit-delete"
                                text: delegate.builtin ? i18n("The default timer cannot be deleted") : i18n("Delete")
                                enabled: !delegate.builtin
                                QQC2.ToolTip.visible: hovered && enabled
                                onClicked: {
                                    deleteDialog.uuid = delegate.uuid;
                                    deleteDialog.presetName = delegate.name;
                                    deleteDialog.open();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
