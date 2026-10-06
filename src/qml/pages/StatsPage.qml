// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.ScrollablePage {
    id: root

    title: i18n("Statistics")
    readonly property bool contentLaidOut: root.visible && mainLayout.visible
                                           && mainLayout.width > 0 && mainLayout.height > 0
                                           && mainLayout.children.length > 0

    actions: [
        Kirigami.Action {
            text: i18n("Export CSV…")
            icon.name: "document-export"
            tooltip: i18n("Export sessions of this period to a CSV file")
            onTriggered: csvDialog.open()
        }
    ]

    Dialogs.FileDialog {
        id: csvDialog
        title: i18n("Export Statistics to CSV")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [i18n("CSV files (*.csv)"), i18n("All files (*)")]
        defaultSuffix: "csv"
        onAccepted: {
            const success = StatsModel.exportCsv(selectedFile);
            if (success) {
                applicationWindow().showPassiveNotification(i18n("Statistics successfully exported"));
            } else {
                // The model explains what went wrong (translated); fall back to a generic text.
                const reason = StatsModel.lastExportError();
                applicationWindow().showPassiveNotification(reason.length > 0 ? reason : i18n("Failed to export statistics"));
            }
        }
    }

    function syncPeriodTab(): void {
        switch (StatsModel.period) {
        case StatsModel.Period.Day:
            periodTabBar.currentIndex = 0;
            break;
        case StatsModel.Period.Week:
            periodTabBar.currentIndex = 1;
            break;
        case StatsModel.Period.Month:
            periodTabBar.currentIndex = 2;
            break;
        case StatsModel.Period.Year:
            periodTabBar.currentIndex = 3;
            break;
        }
    }

    function syncBreakdownTab(): void {
        if (StatsModel.breakdownMode === StatsModel.BreakdownMode.ByTimer) {
            breakdownTabBar.currentIndex = 0;
        } else if (StatsModel.breakdownMode === StatsModel.BreakdownMode.ByCategory) {
            breakdownTabBar.currentIndex = 1;
        } else {
            breakdownTabBar.currentIndex = 2;
        }
    }

    Component.onCompleted: {
        syncPeriodTab();
        syncBreakdownTab();
    }

    Connections {
        target: StatsModel
        function onPeriodChanged() {
            root.syncPeriodTab();
        }
        function onBreakdownModeChanged() {
            root.syncBreakdownTab();
        }
    }

    readonly property color borderColor: Qt.alpha(Kirigami.Theme.textColor, 0.15)

    // Flat Breeze surface used by every block of the page.
    component Surface: Rectangle {
        color: Kirigami.Theme.alternateBackgroundColor
        border.width: 1
        border.color: root.borderColor
        radius: Kirigami.Units.cornerRadius
    }

    // Segmented-control tab button.
    component SegButton: QQC2.TabButton {
        id: segButton
        implicitHeight: Kirigami.Units.gridUnit * 2
        padding: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.largeSpacing
        rightPadding: Kirigami.Units.largeSpacing
        contentItem: QQC2.Label {
            text: segButton.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.weight: segButton.checked ? Font.DemiBold : Font.Normal
            color: segButton.checked ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        }
        background: Rectangle {
            radius: Kirigami.Units.cornerRadius - 1
            color: segButton.checked ? Kirigami.Theme.highlightColor
                 : segButton.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.08) : "transparent"
            Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
        }
    }

    component SegBar: QQC2.TabBar {
        padding: 2
        spacing: 2
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: root.borderColor
            radius: Kirigami.Units.cornerRadius
        }
    }

    // Bar with rounded top corners and a flat bottom.
    component RoundedBar: Item {
        id: bar
        property color color
        readonly property real r: Math.min(Kirigami.Units.cornerRadius, width / 2, height)
        Rectangle {
            anchors.fill: parent
            radius: bar.r
            color: bar.color
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.min(bar.r, bar.height)
            color: bar.color
        }
    }

    component MetricCard: Surface {
        id: card
        required property string title
        required property string iconName
        required property string value
        required property color valueColor
        required property string subtitle
        property real progress: -1 // 0..1 shows a slim progress bar

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        implicitHeight: cardColumn.implicitHeight + Kirigami.Units.largeSpacing * 2

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    radius: width / 2
                    color: Qt.alpha(card.valueColor, 0.16)

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: card.iconName
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        color: card.valueColor
                    }
                }

                QQC2.Label {
                    text: card.title
                    color: Kirigami.Theme.disabledTextColor
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Kirigami.Heading {
                text: card.value
                level: 1
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
                color: card.valueColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                visible: card.progress >= 0
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.smallSpacing + 2
                radius: height / 2
                color: Qt.alpha(Kirigami.Theme.textColor, 0.12)

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, card.progress))
                    height: parent.height
                    radius: height / 2
                    color: card.valueColor
                    Behavior on width {
                        NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                    }
                }
            }

            QQC2.Label {
                text: card.subtitle
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        id: mainLayout
        width: root.availableWidth

        ColumnLayout {
            id: content
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 60
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            spacing: Kirigami.Units.largeSpacing

            // --- Period and Options Header -------------------------------------
            // One row when it fits; the check box moves below the tabs in narrow windows.
            GridLayout {
                id: periodHeader
                Layout.fillWidth: true
                columnSpacing: Kirigami.Units.mediumSpacing
                rowSpacing: Kirigami.Units.smallSpacing
                columns: periodTabBar.implicitWidth + includeInterruptedBox.implicitWidth
                         + Kirigami.Units.gridUnit * 2 <= width ? 3 : 1

                SegBar {
                    id: periodTabBar
                    Layout.fillWidth: periodHeader.columns === 1

                    SegButton {
                        text: i18n("Day")
                        onClicked: StatsModel.period = StatsModel.Period.Day
                    }
                    SegButton {
                        text: i18n("Week")
                        onClicked: StatsModel.period = StatsModel.Period.Week
                    }
                    SegButton {
                        text: i18n("Month")
                        onClicked: StatsModel.period = StatsModel.Period.Month
                    }
                    SegButton {
                        text: i18n("Year")
                        onClicked: StatsModel.period = StatsModel.Period.Year
                    }
                }

                Item {
                    Layout.fillWidth: true
                    visible: periodHeader.columns > 1
                }

                QQC2.CheckBox {
                    id: includeInterruptedBox
                    text: i18n("Include interrupted")
                    checked: StatsModel.includeInterrupted
                    onToggled: StatsModel.includeInterrupted = checked
                }
            }

            // --- Date Navigation Row -------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    level: 2
                    text: StatsModel.title
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    color: "transparent"
                    border.width: 1
                    border.color: root.borderColor
                    radius: Kirigami.Units.cornerRadius
                    implicitWidth: navRow.implicitWidth + 2
                    implicitHeight: navRow.implicitHeight + 2

                    RowLayout {
                        id: navRow
                        anchors.centerIn: parent
                        spacing: 0

                        QQC2.ToolButton {
                            icon.name: "go-previous"
                            text: i18n("Previous Period")
                            display: QQC2.AbstractButton.IconOnly
                            flat: true
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: StatsModel.previousPeriod()
                        }

                        Kirigami.Separator {
                            Layout.fillHeight: true
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                        }

                        QQC2.ToolButton {
                            text: i18n("Today")
                            icon.name: "go-jump-today"
                            display: QQC2.AbstractButton.TextBesideIcon
                            flat: true
                            enabled: !StatsModel.isCurrent
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: StatsModel.today()
                        }

                        Kirigami.Separator {
                            Layout.fillHeight: true
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                        }

                        QQC2.ToolButton {
                            icon.name: "go-next"
                            text: i18n("Next Period")
                            display: QQC2.AbstractButton.IconOnly
                            flat: true
                            enabled: StatsModel.canGoNext
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: StatsModel.nextPeriod()
                        }
                    }
                }
            }

            // --- Summary Metric Cards ------------------------------------------
            GridLayout {
                Layout.fillWidth: true
                columns: content.width >= Kirigami.Units.gridUnit * 40 ? 4
                         : content.width >= Kirigami.Units.gridUnit * 18 ? 2 : 1
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing

                MetricCard {
                    title: i18n("Work Time")
                    iconName: "chronometer"
                    value: StatsModel.formatDuration(StatsModel.workSeconds)
                    valueColor: Kirigami.Theme.highlightColor
                    subtitle: {
                        if (StatsModel.includeInterrupted && StatsModel.interruptedWorkSessions > 0) {
                            return i18nc("@info completed sessions (interrupted sessions)", "%1 (%2)",
                                         i18np("%1 completed", "%1 completed", StatsModel.completedWorkSessions),
                                         i18np("%1 interrupted", "%1 interrupted", StatsModel.interruptedWorkSessions));
                        }
                        return i18np("%1 completed session", "%1 completed sessions", StatsModel.completedWorkSessions);
                    }
                }

                MetricCard {
                    title: i18n("Break Time")
                    iconName: "food-cup"
                    value: StatsModel.formatDuration(StatsModel.breakSeconds)
                    valueColor: Kirigami.Theme.positiveTextColor
                    subtitle: i18n("Ratio: %1", StatsModel.formatRatio(StatsModel.ratio))
                }

                MetricCard {
                    title: i18n("Daily Goal")
                    iconName: "emblem-favorite"
                    value: AppSettings.dailyGoal > 0
                           ? i18n("%1 / %2", StatsModel.todayCompletedCount, AppSettings.dailyGoal)
                           : i18n("Disabled")
                    valueColor: StatsModel.dailyGoalReached
                                ? Kirigami.Theme.positiveTextColor
                                : AppSettings.dailyGoal > 0 ? Kirigami.Theme.neutralTextColor
                                                            : Kirigami.Theme.disabledTextColor
                    progress: AppSettings.dailyGoal > 0 ? StatsModel.dailyGoalProgress : -1
                    subtitle: {
                        if (AppSettings.dailyGoal <= 0) {
                            return i18n("No daily goal set");
                        }
                        if (StatsModel.dailyGoalReached) {
                            return i18n("Target reached today! 🎉");
                        }
                        return i18n("%1% of daily target", Math.round(StatsModel.dailyGoalProgress * 100));
                    }
                }

                MetricCard {
                    title: i18n("Daily Streak")
                    iconName: "rating"
                    value: i18np("%1 day", "%1 days", StatsModel.currentStreak)
                    valueColor: StatsModel.currentStreak > 0 ? Kirigami.Theme.neutralTextColor
                                                             : Kirigami.Theme.disabledTextColor
                    subtitle: i18np("Best streak: %1 day", "Best streak: %1 days", StatsModel.bestStreak)
                }
            }

            // --- Activity Chart ------------------------------------------------
            Surface {
                Layout.fillWidth: true
                implicitHeight: activityColumn.implicitHeight + Kirigami.Units.largeSpacing * 2

                ColumnLayout {
                    id: activityColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        Kirigami.Heading {
                            level: 3
                            text: i18n("Activity")
                            Layout.fillWidth: true
                        }

                        // Legend
                        RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Rectangle {
                                implicitWidth: Kirigami.Units.smallSpacing * 2
                                implicitHeight: implicitWidth
                                radius: width / 2
                                color: Kirigami.Theme.highlightColor
                            }
                            QQC2.Label {
                                text: i18n("Work")
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Item { implicitWidth: Kirigami.Units.smallSpacing }
                            Rectangle {
                                implicitWidth: Kirigami.Units.smallSpacing * 2
                                implicitHeight: implicitWidth
                                radius: width / 2
                                color: Kirigami.Theme.positiveTextColor
                            }
                            QQC2.Label {
                                text: i18n("Break")
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    Item {
                        id: chart
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 14

                        readonly property font smallBoldFont: {
                            let f = Kirigami.Theme.smallFont;
                            f.bold = true;
                            return f;
                        }
                        readonly property bool hasData: StatsModel.workSeconds + StatsModel.breakSeconds > 0
                        readonly property real axisWidth: Kirigami.Units.gridUnit * 3.5
                        readonly property real axisSpacing: Kirigami.Units.smallSpacing
                        readonly property real plotTop: Kirigami.Units.gridUnit * 0.5
                        readonly property real xLabelHeight: Kirigami.Units.gridUnit * 1.5
                        readonly property real plotHeight: Math.max(0, height - plotTop - xLabelHeight)
                        readonly property int bucketCount: Math.max(1, StatsModel.buckets.length)
                        readonly property real bucketWidth: Math.max(Kirigami.Units.gridUnit * 1.25,
                                                                     chartFlickable.width / bucketCount)
                        // Show only every n-th x label when they would collide (24 hours: 1, 2, 3, 6 or 12).
                        readonly property int labelStep: {
                            const need = Math.ceil((Kirigami.Units.gridUnit * 2.4) / bucketWidth);
                            if (bucketCount === 24) {
                                const steps = [1, 2, 3, 6, 12];
                                for (let i = 0; i < steps.length; ++i) {
                                    if (steps[i] >= need) {
                                        return steps[i];
                                    }
                                }
                                return 12;
                            }
                            return Math.max(1, need);
                        }

                        // Gridlines at top, middle and baseline
                        Item {
                            anchors.fill: parent
                            anchors.leftMargin: chart.axisWidth + chart.axisSpacing
                            anchors.topMargin: chart.plotTop
                            anchors.bottomMargin: chart.xLabelHeight

                            Kirigami.Separator {
                                anchors.top: parent.top
                                width: parent.width
                                opacity: 0.25
                            }
                            Kirigami.Separator {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                opacity: 0.25
                            }
                            Kirigami.Separator {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                opacity: 0.6
                            }
                        }

                        // Y axis labels, each centred on its gridline
                        Item {
                            width: chart.axisWidth
                            height: parent.height

                            QQC2.Label {
                                width: parent.width
                                y: chart.plotTop - height / 2
                                horizontalAlignment: Text.AlignRight
                                text: StatsModel.formatDuration(StatsModel.maxBucketSeconds)
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideLeft
                                visible: chart.hasData
                            }
                            QQC2.Label {
                                width: parent.width
                                y: chart.plotTop + chart.plotHeight / 2 - height / 2
                                horizontalAlignment: Text.AlignRight
                                text: StatsModel.formatDuration(StatsModel.maxBucketSeconds / 2)
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideLeft
                                visible: chart.hasData
                            }
                            QQC2.Label {
                                width: parent.width
                                y: chart.plotTop + chart.plotHeight - height / 2
                                horizontalAlignment: Text.AlignRight
                                text: StatsModel.formatDuration(0)
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideLeft
                            }
                        }

                        // Bars
                        Flickable {
                            id: chartFlickable
                            anchors.fill: parent
                            anchors.leftMargin: chart.axisWidth + chart.axisSpacing
                            contentWidth: chart.bucketWidth * chart.bucketCount
                            contentHeight: height
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds

                            Row {
                                id: chartRow
                                height: parent.height

                                Repeater {
                                    model: StatsModel.buckets

                                    delegate: Item {
                                        id: bucketItem
                                        required property var modelData
                                        required property int index

                                        width: chart.bucketWidth
                                        height: chartRow.height

                                        readonly property real maxSec: StatsModel.maxBucketSeconds > 0 ? StatsModel.maxBucketSeconds : 3600
                                        readonly property real workHeight: Math.min(chart.plotHeight, (modelData.workSeconds / maxSec) * chart.plotHeight)
                                        readonly property real breakHeight: Math.min(chart.plotHeight, (modelData.breakSeconds / maxSec) * chart.plotHeight)
                                        readonly property real barWidth: Math.max(2, Math.min(Kirigami.Units.gridUnit * 1.1,
                                                                                              (width - Kirigami.Units.smallSpacing * 2) / 2 - 1))
                                        readonly property bool showLabel: index % chart.labelStep === 0
                                                                          || (modelData.isCurrent && chart.labelStep === 1)

                                        Accessible.role: Accessible.StaticText
                                        Accessible.name: modelData.label
                                        Accessible.description: modelData.tooltip

                                        // Column highlight: current bucket (accent) or hovered bucket (subtle)
                                        Rectangle {
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            anchors.bottomMargin: chart.xLabelHeight
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: parent.width - 2
                                            radius: Kirigami.Units.cornerRadius
                                            color: modelData.isCurrent ? Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
                                                                       : Qt.alpha(Kirigami.Theme.textColor, 0.06)
                                            opacity: modelData.isCurrent || bucketMouse.containsMouse ? 1 : 0
                                            Behavior on opacity {
                                                NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
                                            }
                                        }

                                        Row {
                                            anchors.bottom: parent.bottom
                                            anchors.bottomMargin: chart.xLabelHeight
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            spacing: 2

                                            RoundedBar {
                                                width: bucketItem.barWidth
                                                height: modelData.workSeconds > 0 ? Math.max(4, bucketItem.workHeight) : 0
                                                anchors.bottom: parent.bottom
                                                color: Kirigami.Theme.highlightColor
                                            }

                                            RoundedBar {
                                                width: bucketItem.barWidth
                                                height: modelData.breakSeconds > 0 ? Math.max(4, bucketItem.breakHeight) : 0
                                                anchors.bottom: parent.bottom
                                                color: Kirigami.Theme.positiveTextColor
                                            }
                                        }

                                        QQC2.Label {
                                            anchors.bottom: parent.bottom
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            height: chart.xLabelHeight
                                            verticalAlignment: Text.AlignVCenter
                                            text: modelData.label
                                            visible: bucketItem.showLabel
                                            font: modelData.isCurrent ? chart.smallBoldFont : Kirigami.Theme.smallFont
                                            color: modelData.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                                        }

                                        QQC2.ToolTip.visible: bucketMouse.containsMouse
                                        QQC2.ToolTip.text: modelData.tooltip
                                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                                        MouseArea {
                                            id: bucketMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                        }
                                    }
                                }
                            }
                        }

                        // Empty state
                        QQC2.Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.horizontalCenterOffset: (chart.axisWidth + chart.axisSpacing) / 2
                            y: chart.plotTop + chart.plotHeight / 2 - height
                            visible: !chart.hasData
                            text: i18n("No activity recorded for this period.")
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                }
            }

            // --- Breakdown Section ---------------------------------------------
            Surface {
                Layout.fillWidth: true
                implicitHeight: distributionContent.implicitHeight + Kirigami.Units.largeSpacing * 2

                ColumnLayout {
                    id: distributionContent
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    // One row when it fits; the tabs move below the heading in narrow windows.
                    GridLayout {
                        id: distributionHeader
                        Layout.fillWidth: true
                        columnSpacing: Kirigami.Units.mediumSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        columns: distributionHeading.implicitWidth + breakdownTabBar.implicitWidth
                                 + Kirigami.Units.gridUnit * 2 <= width ? 2 : 1

                        Kirigami.Heading {
                            id: distributionHeading
                            level: 3
                            text: i18n("Distribution")
                            Layout.fillWidth: true
                        }

                        SegBar {
                            id: breakdownTabBar
                            Layout.fillWidth: distributionHeader.columns === 1

                            SegButton {
                                text: i18n("By Timer")
                                onClicked: StatsModel.breakdownMode = StatsModel.BreakdownMode.ByTimer
                            }
                            SegButton {
                                text: i18n("By Category")
                                onClicked: StatsModel.breakdownMode = StatsModel.BreakdownMode.ByCategory
                            }
                            SegButton {
                                text: i18n("By Task")
                                onClicked: StatsModel.breakdownMode = StatsModel.BreakdownMode.ByTask
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.largeSpacing
                        Layout.bottomMargin: Kirigami.Units.largeSpacing
                        visible: StatsModel.breakdown.length === 0
                        text: i18n("No distribution data recorded for this period.")
                        icon.name: "view-history"
                    }

                    Repeater {
                        model: StatsModel.breakdown

                        delegate: ColumnLayout {
                            id: rowItem
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.topMargin: index > 0 ? Kirigami.Units.smallSpacing : 0
                            spacing: Kirigami.Units.smallSpacing

                            readonly property color rankColor: Qt.alpha(Kirigami.Theme.highlightColor,
                                                                        Math.max(0.35, 1 - index * 0.2))

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Rectangle {
                                    implicitWidth: Kirigami.Units.smallSpacing * 2
                                    implicitHeight: implicitWidth
                                    radius: width / 2
                                    color: rowItem.rankColor
                                }

                                QQC2.Label {
                                    text: rowItem.modelData.name
                                    font.weight: Font.Bold
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                QQC2.Label {
                                    text: rowItem.modelData.formattedDuration
                                    font.weight: Font.DemiBold
                                    font.features: { "tnum": 1 }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.largeSpacing

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Kirigami.Units.smallSpacing + 4
                                    radius: height / 2
                                    color: Qt.alpha(Kirigami.Theme.textColor, 0.1)

                                    Rectangle {
                                        width: parent.width * Math.max(0, Math.min(100, rowItem.modelData.percentage)) / 100
                                        height: parent.height
                                        radius: height / 2
                                        color: rowItem.rankColor
                                        Behavior on width {
                                            NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                                        }
                                    }
                                }

                                QQC2.Label {
                                    text: i18nc("@info sessions • percentage of total", "%1 • %2",
                                                i18np("%1 session", "%1 sessions", rowItem.modelData.sessions),
                                                rowItem.modelData.formattedPercentage)
                                    color: Kirigami.Theme.disabledTextColor
                                    font: Kirigami.Theme.smallFont
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
