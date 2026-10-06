// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.ScrollablePage {
    id: aboutPage

    title: i18n("About")

    readonly property var aboutData: typeof appAboutData !== "undefined" ? appAboutData : ({
        displayName: i18n("kTomato"),
        shortDescription: i18n("A Pomodoro timer with time statistics"),
        version: "",
        copyrightStatement: ""
    })

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

    component SecondaryLabel: QQC2.Label {
        Layout.fillWidth: true
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }

    // Set when the user closes the update status message; cleared by every new check.
    property bool updateStatusDismissed: false

    Connections {
        target: UpdateChecker
        function onStatusChanged() {
            if (UpdateChecker.checking) {
                aboutPage.updateStatusDismissed = false;
            }
        }
    }

    ColumnLayout {
        id: content
        width: aboutPage.availableWidth
        spacing: Kirigami.Units.largeSpacing

        // Everything lives in a centred column of consistent maximum width.
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 40
            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            // Hero
            ColumnLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: "io.github.mineraleyt.ktomato"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.enormous
                    Layout.preferredHeight: Kirigami.Units.iconSizes.enormous
                }

                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    text: aboutPage.aboutData.displayName
                    level: 1
                    type: Kirigami.Heading.Type.Primary
                }

                Rectangle {
                    visible: aboutPage.aboutData.version.length > 0
                    Layout.alignment: Qt.AlignHCenter
                    radius: height / 2
                    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.15)
                    border.width: 1
                    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.4)
                    implicitWidth: versionLabel.implicitWidth + Kirigami.Units.gridUnit
                    implicitHeight: versionLabel.implicitHeight + Kirigami.Units.smallSpacing

                    QQC2.Label {
                        id: versionLabel
                        anchors.centerIn: parent
                        text: i18n("Version %1", aboutPage.aboutData.version)
                        font: Kirigami.Theme.smallFont
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: aboutPage.aboutData.shortDescription
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            Card {
                title: i18n("Updates")

                // Update card styled after Discover when an update is available
                Rectangle {
                    id: updateCard
                    visible: UpdateChecker.hasUpdate
                    Layout.fillWidth: true
                    radius: Kirigami.Units.cornerRadius
                    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
                    border.width: 1
                    border.color: Kirigami.Theme.highlightColor
                    implicitHeight: updateCardLayout.implicitHeight + Kirigami.Units.largeSpacing * 2

                    RowLayout {
                        id: updateCardLayout
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            margins: Kirigami.Units.largeSpacing
                        }
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: "update-low"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                            color: Kirigami.Theme.highlightColor
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: i18n("A newer version (%1) of kTomato is available.", UpdateChecker.latestVersion)
                            wrapMode: Text.WordWrap
                        }

                        QQC2.Button {
                            icon.name: "download-symbolic"
                            text: i18n("View Release")
                            highlighted: true
                            onClicked: Qt.openUrlExternally(UpdateChecker.releaseUrl)
                        }
                    }
                }

                // Inline message for up-to-date or error states
                Kirigami.InlineMessage {
                    id: updateStatusMsg
                    Layout.fillWidth: true
                    readonly property bool shouldShow: !aboutPage.updateStatusDismissed
                                                       && (UpdateChecker.status === UpdateChecker.UpToDate
                                                           || UpdateChecker.status === UpdateChecker.Error)
                    visible: shouldShow
                    showCloseButton: true
                    // The close button assigns visible = false, which removes the binding: remember the
                    // dismissal and bind again, so the result of the next check is shown.
                    onVisibleChanged: {
                        if (!visible && shouldShow) {
                            aboutPage.updateStatusDismissed = true;
                            visible = Qt.binding(() => updateStatusMsg.shouldShow);
                        }
                    }
                    type: UpdateChecker.status === UpdateChecker.Error
                        ? Kirigami.MessageType.Warning
                        : Kirigami.MessageType.Information
                    text: {
                        if (UpdateChecker.status === UpdateChecker.UpToDate) {
                            return i18n("kTomato is up to date (version %1).", UpdateChecker.currentVersion);
                        } else if (UpdateChecker.status === UpdateChecker.Error) {
                            return i18n("Could not check for updates: %1", UpdateChecker.errorMessage);
                        }
                        return "";
                    }
                    actions: [
                        Kirigami.Action {
                            text: i18n("Open Releases Page")
                            icon.name: "globe-symbolic"
                            visible: UpdateChecker.status === UpdateChecker.Error
                            onTriggered: Qt.openUrlExternally("https://github.com/MineraleYT/kTomato/releases")
                        }
                    ]
                }

                QQC2.Button {
                    id: checkUpdatesBtn
                    icon.name: "system-software-update"
                    text: UpdateChecker.checking ? i18n("Checking for updates…") : i18n("Check for Updates")
                    enabled: !UpdateChecker.checking
                    onClicked: {
                        aboutPage.updateStatusDismissed = false;
                        UpdateChecker.checkForUpdates();
                    }
                }
            }

            Card {
                title: i18n("Links")

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        flat: true
                        icon.name: "applications-development-symbolic"
                        text: i18n("Report a bug")
                        onClicked: Qt.openUrlExternally("https://github.com/MineraleYT/kTomato/issues")
                    }

                    QQC2.Button {
                        flat: true
                        icon.name: "globe-symbolic"
                        text: i18n("Project website")
                        onClicked: Qt.openUrlExternally("https://github.com/MineraleYT/kTomato")
                    }
                }
            }

            Card {
                title: i18n("License & credits")

                QQC2.Label {
                    Layout.fillWidth: true
                    text: aboutPage.aboutData.copyrightStatement
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                }

                SecondaryLabel {
                    text: i18n("Licensed under GNU General Public License, version 3 or later.")
                }

                SecondaryLabel {
                    text: i18n("kTomato is an independent project and is not an official KDE application. It uses KDE Frameworks libraries.")
                }
            }
        }
    }
}
