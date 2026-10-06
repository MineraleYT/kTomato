// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.Page {
    id: page

    title: i18n("Timer")

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        visible: ScreenLockWatcher.pausedByScreenLock && TimerEngine.paused
        type: Kirigami.MessageType.Information
        text: i18n("The timer was paused because your screen was locked.")
        actions: [
            Kirigami.Action {
                text: i18n("Resume")
                icon.name: "media-playback-start"
                onTriggered: {
                    ScreenLockWatcher.clearLockPauseNotice();
                    TimerEngine.resume();
                }
            },
            Kirigami.Action {
                text: i18n("Dismiss")
                icon.name: "dialog-cancel"
                onTriggered: ScreenLockWatcher.clearLockPauseNotice()
            }
        ]
    }

    readonly property bool isIdle: TimerEngine.state === TimerEngine.State.Idle
    readonly property bool isWorkPhase: TimerEngine.phase === TimerEngine.Phase.Work

    readonly property color phaseColor: {
        switch (TimerEngine.phase) {
        case TimerEngine.Phase.Work:
            return Kirigami.Theme.negativeTextColor;
        case TimerEngine.Phase.ShortBreak:
            return Kirigami.Theme.positiveTextColor;
        default:
            return Kirigami.Theme.highlightColor;
        }
    }

    // While working, the chip carries the timer's category (e.g. "Study"); "Work" when it has none.
    readonly property string phaseTitle: {
        switch (TimerEngine.phase) {
        case TimerEngine.Phase.Work: {
            const category = PresetModel.currentCategory.trim();
            return category.length > 0 ? category : i18n("Work");
        }
        case TimerEngine.Phase.ShortBreak:
            return i18n("Short break");
        default:
            return i18n("Long break");
        }
    }

    readonly property string statusText: {
        if (TimerEngine.paused) {
            return i18n("Paused");
        }
        if (isIdle) {
            return isWorkPhase ? i18n("Ready to focus") : i18n("Time for a break");
        }
        return isWorkPhase ? i18n("Focus time") : i18n("Break");
    }

    // The chip only shows once the timer has started (running or paused).
    readonly property bool chipShown: !isIdle

    // The chip is neutral while paused, otherwise it carries the phase colour.
    readonly property color chipColor: TimerEngine.paused ? Kirigami.Theme.neutralTextColor : phaseColor

    readonly property string primaryText: {
        if (TimerEngine.paused) {
            return i18n("Resume");
        }
        if (isIdle) {
            return isWorkPhase ? i18n("Start") : i18n("Start break");
        }
        return i18n("Pause");
    }

    readonly property string primaryIcon: (isIdle || TimerEngine.paused) ? "media-playback-start" : "media-playback-pause"

    readonly property int cycleNumber: {
        const total = TimerEngine.cyclesBeforeLong;
        const done = TimerEngine.completedCycles;
        return Math.min(total, isWorkPhase ? done + 1 : Math.max(1, done));
    }

    readonly property real ringSize: Math.max(Kirigami.Units.gridUnit * 10,
                                              Math.min(Kirigami.Units.gridUnit * 19,
                                                       page.availableWidth - Kirigami.Units.gridUnit * 2,
                                                       page.availableHeight - Kirigami.Units.gridUnit * 19))

    readonly property int maxCycleDots: 12
    readonly property string presetLockedText: i18n("Stop the timer to switch to a different timer.")

    function formatTime(totalSeconds: int): string {
        const h = Math.floor(totalSeconds / 3600);
        const m = Math.floor((totalSeconds % 3600) / 60);
        const s = totalSeconds % 60;
        const pad = n => String(n).padStart(2, "0");
        return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : pad(m) + ":" + pad(s);
    }

    // Centered when there is room; scrolls when the window is too short (e.g. at the minimum
    // size with the screen lock message shown).
    Flickable {
        id: flickable
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: Math.max(height, content.implicitHeight)
        boundsBehavior: Flickable.StopAtBounds
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

        Item {
            width: flickable.width
            height: flickable.contentHeight

            ColumnLayout {
                id: content
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - Kirigami.Units.gridUnit * 2)
                spacing: Kirigami.Units.largeSpacing

                // --- Preset Selector Dropdown ---
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing

                    // The selector is disabled while a timer runs; a disabled control shows no tooltip,
                    // so the explanation is shown from here.
                    HoverHandler {
                        id: presetHover
                    }

                    QQC2.ToolTip.visible: TimerEngine.active && presetHover.hovered
                    QQC2.ToolTip.text: page.presetLockedText
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                    Kirigami.Icon {
                        source: PresetModel.currentIconName
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        color: Kirigami.Theme.highlightColor
                    }

                    QQC2.ComboBox {
                        id: presetBox
                        Layout.preferredWidth: Math.min(Kirigami.Units.gridUnit * 14,
                                                        page.availableWidth - Kirigami.Units.gridUnit * 4)
                        flat: true
                        model: PresetModel
                        textRole: "name"
                        valueRole: "uuid"
                        enabled: !TimerEngine.active

                        displayText: i18nc("@item:inlistbox timer name (work minutes)", "%1 (%2 min)",
                                           PresetModel.currentName, PresetModel.currentWorkMinutes)
                        Accessible.name: i18n("Timer")
                        Accessible.description: TimerEngine.active ? page.presetLockedText : ""

                        popup.width: presetBox.width
                        popup.x: 0

                        delegate: QQC2.ItemDelegate {
                            id: itemDelegate
                            required property int index
                            required property string uuid
                            required property string name
                            required property string category
                            required property string summary
                            required property string iconName
                            required property int workMinutes
                            required property int shortBreakSeconds

                            width: ListView.view ? ListView.view.width : (presetBox.popup ? presetBox.popup.availableWidth : presetBox.width)
                            highlighted: PresetModel.currentUuid === uuid
                            // Not drawn (custom contentItem), but read by screen readers.
                            text: name

                            background: Rectangle {
                                color: itemDelegate.highlighted
                                    ? Kirigami.Theme.highlightColor
                                    : (itemDelegate.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.08) : "transparent")
                                radius: Kirigami.Units.cornerRadius
                            }

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: itemDelegate.iconName
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                    color: itemDelegate.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: itemDelegate.name
                                    font.bold: itemDelegate.highlighted
                                    color: itemDelegate.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: itemDelegate.category.length > 0
                                    radius: height / 2
                                    color: itemDelegate.highlighted
                                        ? Qt.alpha(Kirigami.Theme.highlightedTextColor, 0.25)
                                        : Qt.alpha(Kirigami.Theme.textColor, 0.1)
                                    implicitWidth: catBadge.implicitWidth + Kirigami.Units.smallSpacing
                                    implicitHeight: catBadge.implicitHeight + 2

                                    QQC2.Label {
                                        id: catBadge
                                        anchors.centerIn: parent
                                        text: itemDelegate.category
                                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                        color: itemDelegate.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                                    }
                                }

                                QQC2.Label {
                                    text: i18nc("%1m work, %2m break", "%1m / %2m", itemDelegate.workMinutes, Math.round(itemDelegate.shortBreakSeconds / 60))
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: itemDelegate.highlighted ? Qt.alpha(Kirigami.Theme.highlightedTextColor, 0.85) : Kirigami.Theme.disabledTextColor
                                }
                            }
                            // Clicks are handled by the ComboBox itself: it sets currentIndex, emits
                            // activated() and closes the popup.
                        }

                        onActivated: PresetModel.currentUuid = currentValue

                        Binding {
                            target: presetBox
                            property: "currentIndex"
                            value: PresetModel.currentIndex
                        }
                    }
                }

                // Phase chip
                Rectangle {
                    id: phaseChip
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: chipRow.implicitWidth + Kirigami.Units.gridUnit
                    implicitHeight: chipRow.implicitHeight + Kirigami.Units.smallSpacing * 1.5
                    radius: height / 2
                    color: Qt.alpha(page.chipColor, 0.16)
                    border.width: 1
                    border.color: Qt.alpha(page.chipColor, 0.35)
                    Behavior on color { ColorAnimation { duration: Kirigami.Units.longDuration } }

                    // Hidden, not removed, while idle: the ring must not jump when the timer starts.
                    opacity: page.chipShown ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic } }

                    Accessible.ignored: !page.chipShown
                    Accessible.role: Accessible.StaticText
                    Accessible.name: page.phaseTitle

                    RowLayout {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            implicitWidth: Kirigami.Units.smallSpacing * 1.5
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: page.chipColor
                        }
                        QQC2.Label {
                            text: page.phaseTitle
                            font.weight: Font.DemiBold
                            color: page.chipColor
                        }
                    }
                }

                ProgressRing {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: page.ringSize
                    Layout.preferredHeight: page.ringSize
                    progress: TimerEngine.progress
                    color: page.phaseColor

                    // The digits are pinned to the ring's center. The status line hangs below them, so
                    // its text coming and going never moves the clock (a centered column would).
                    QQC2.Label {
                        id: countdown
                        anchors.centerIn: parent
                        text: page.formatTime(TimerEngine.remainingSeconds)
                        font.pixelSize: page.ringSize * 0.22
                        font.features: { "tnum": 1 } // fixed-width digits: no jitter while counting
                        font.weight: Font.Light
                        opacity: TimerEngine.paused ? 0.55 : 1
                        Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic } }
                    }

                    QQC2.Label {
                        anchors.top: countdown.bottom
                        anchors.topMargin: -Kirigami.Units.smallSpacing
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: page.statusText
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 1.5
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                // Cycle progress: completed pills are filled, the current one is outlined in the phase colour.
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing
                    // With many cycles the pills would not fit: the text line below is enough then.
                    visible: TimerEngine.cyclesBeforeLong > 0 && TimerEngine.cyclesBeforeLong <= page.maxCycleDots

                    Repeater {
                        model: TimerEngine.cyclesBeforeLong

                        Rectangle {
                            id: cyclePill
                            required property int index
                            readonly property bool done: index < TimerEngine.completedCycles
                            readonly property bool current: !done && index === TimerEngine.completedCycles && TimerEngine.active
                            implicitWidth: Kirigami.Units.gridUnit * (current ? 1.8 : 1.2)
                            implicitHeight: Kirigami.Units.smallSpacing * 1.5
                            radius: height / 2
                            color: done ? page.phaseColor
                                 : current ? Qt.alpha(page.phaseColor, 0.45)
                                 : Qt.alpha(Kirigami.Theme.textColor, 0.15)
                            Behavior on color { ColorAnimation { duration: Kirigami.Units.longDuration } }
                            Behavior on implicitWidth { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: -Kirigami.Units.smallSpacing
                    visible: TimerEngine.cyclesBeforeLong > 0
                    text: i18n("Cycle %1 of %2", page.cycleNumber, TimerEngine.cyclesBeforeLong)
                    color: Kirigami.Theme.disabledTextColor
                }

                // Daily goal: slim bar with the count.
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(Kirigami.Units.gridUnit * 14, page.availableWidth - Kirigami.Units.gridUnit * 2)
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing
                    visible: AppSettings.dailyGoal > 0

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            visible: StatsModel.dailyGoalReached
                            source: "emblem-favorite"
                            fallback: "favorite"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            color: Kirigami.Theme.positiveTextColor
                        }

                        QQC2.Label {
                            text: StatsModel.dailyGoalReached
                                ? i18n("Daily goal reached! (%1/%2)", StatsModel.todayCompletedCount, AppSettings.dailyGoal)
                                : i18n("Today's goal: %1 of %2", StatsModel.todayCompletedCount, AppSettings.dailyGoal)
                            font.bold: StatsModel.dailyGoalReached
                            color: StatsModel.dailyGoalReached ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Kirigami.Units.smallSpacing
                        radius: height / 2
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.15)

                        Rectangle {
                            height: parent.height
                            radius: height / 2
                            width: parent.width * Math.min(1, AppSettings.dailyGoal > 0
                                                              ? StatsModel.todayCompletedCount / AppSettings.dailyGoal : 0)
                            color: StatsModel.dailyGoalReached ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.highlightColor
                            Behavior on width { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                // Reminder that the desktop is not showing notifications because of us. Its space is
                // reserved whenever silencing is possible, so showing it never moves the rest of the page.
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing
                    visible: SilenceController.available
                    opacity: SilenceController.active ? 1 : 0

                    Kirigami.Icon {
                        source: "notifications-disabled"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        color: Kirigami.Theme.disabledTextColor
                    }
                    QQC2.Label {
                        text: i18n("Notifications silenced until the break")
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Button {
                        // Fixed width: "Start", "Pause", "Resume" and "Start break" must not push
                        // Stop and Skip sideways.
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.6
                        text: page.primaryText
                        icon.name: page.primaryIcon
                        icon.width: Kirigami.Units.iconSizes.smallMedium
                        icon.height: Kirigami.Units.iconSizes.smallMedium
                        font.weight: Font.DemiBold
                        highlighted: true
                        onClicked: TimerEngine.toggle()

                        // Explicit accent fill so the primary action stands out in every style.
                        background: Rectangle {
                            radius: Kirigami.Units.cornerRadius
                            color: Kirigami.Theme.highlightColor
                            opacity: parent.down ? 0.8 : (parent.hovered ? 0.92 : 1)
                            border.width: parent.visualFocus ? 2 : 0
                            border.color: Kirigami.Theme.textColor
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Item { Layout.fillWidth: true }
                            Kirigami.Icon {
                                source: page.primaryIcon
                                color: Kirigami.Theme.highlightedTextColor
                                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                            }
                            QQC2.Label {
                                text: page.primaryText
                                font.weight: Font.DemiBold
                                color: Kirigami.Theme.highlightedTextColor
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }

                    QQC2.Button {
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.6
                        text: i18n("Stop")
                        icon.name: "media-playback-stop"
                        flat: true
                        enabled: TimerEngine.active
                        onClicked: TimerEngine.stop()
                    }

                    QQC2.Button {
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.6
                        text: i18n("Skip")
                        icon.name: "media-skip-forward"
                        flat: true
                        enabled: TimerEngine.active
                        onClicked: TimerEngine.skip()
                    }
                }
            }
        }
    }
}
