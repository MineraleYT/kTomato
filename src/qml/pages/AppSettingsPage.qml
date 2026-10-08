// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

/// Settings of the program itself. The options of each timer live on that timer's own page.
Kirigami.ScrollablePage {
    id: page

    title: i18n("Settings")

    /// Index of the entry of `list` (an array of { value } objects) whose value is `value`;
    /// 0 when there is none. Used by combo box bindings, which re-evaluate when the
    /// translated model is rebuilt after a language change.
    function valueIndex(list: var, value: var): int {
        for (let i = 0; i < list.length; ++i) {
            if (list[i].value === value) {
                return i;
            }
        }
        return 0;
    }

    // Calendar sync state, guarded because the singleton can be gone at shutdown.
    readonly property bool calOn: CalendarSync ? CalendarSync.enabled : false
    readonly property string calProvider: CalendarSync ? CalendarSync.provider : "nextcloud"
    readonly property bool calCustom: calProvider === "caldav"
    readonly property bool calHasPassword: CalendarSync ? CalendarSync.hasPassword : false
    readonly property bool calConnected: calHasPassword && !calCustom
    readonly property int calStatus: CalendarSync ? CalendarSync.status : 0
    readonly property bool calWaiting: CalendarSync ? calStatus === CalendarSync.WaitingForBrowser : false
    readonly property int calPending: CalendarSync ? CalendarSync.pendingCount : 0
    // Signed in with something to send events to: password and user name are stored.
    readonly property bool calConfigured: calHasPassword && CalendarSync !== null && CalendarSync.username.length > 0
    // While the server is being asked, only the button that started the request is greyed out
    // (the two buttons used to dim together, which looked like both had been pressed).
    property string calAction: ""   // "refresh" or "test"
    readonly property bool calWorking: CalendarSync ? calStatus === CalendarSync.Working : false
    readonly property bool calRefreshing: calWorking && calAction === "refresh"
    readonly property bool calTesting: calWorking && calAction === "test"
    // A successful connection test is shown on the Test button itself (a green check for a few
    // seconds); failures are explained in the message at the bottom of the card.
    property bool calTestOk: false
    onCalWorkingChanged: {
        if (calWorking) {
            calTestOk = false;
        } else {
            if (calAction === "test" && CalendarSync && calStatus === CalendarSync.Ready) {
                calTestOk = true;
                calTestOkTimer.restart();
            }
            calAction = "";
        }
    }
    Timer {
        id: calTestOkTimer
        interval: 3000
        onTriggered: page.calTestOk = false
    }
    readonly property string calLastSuccess: {
        const d = CalendarSync ? CalendarSync.lastSuccess : null;
        if (!d) {
            return "";
        }
        const date = new Date(d);
        return isNaN(date.getTime()) ? "" : date.toLocaleString(Qt.locale(), Locale.ShortFormat);
    }
    // Calendars to pick from; the saved one alone when no list has been fetched yet.
    readonly property var calChoices: {
        if (!CalendarSync) {
            return [];
        }
        const list = CalendarSync.calendars;
        if (list && list.length > 0) {
            return list;
        }
        if (CalendarSync.calendarUrl.length > 0) {
            return [{ name: CalendarSync.calendarName.length > 0 ? CalendarSync.calendarName : CalendarSync.calendarUrl,
                      url: CalendarSync.calendarUrl, color: "" }];
        }
        return [];
    }
    function calChoiceIndex(): int {
        const url = CalendarSync ? CalendarSync.calendarUrl : "";
        for (let i = 0; i < calChoices.length; ++i) {
            if (calChoices[i].url === url) {
                return i;
            }
        }
        return -1;
    }

    // The calendar list is not kept between runs, and calendars made on the server since the last
    // visit should show up: reload it when the page opens, if the calendar is on and signed in.
    Component.onCompleted: {
        if (CalendarSync && CalendarSync.enabled && CalendarSync.hasPassword && !calWaiting) {
            CalendarSync.refreshCalendars();
        }
    }

    // Result of the last diagnostics action: "", "copied", "exported" or "exportFailed".
    property string diagnosticsResult: ""
    property bool diagnosticsDismissed: false
    // Translated reason of the last failed export, from Diagnostics.lastExportError().
    property string diagnosticsExportError: ""

    function showDiagnosticsResult(result: string): void {
        diagnosticsResult = result;
        diagnosticsDismissed = false;
    }

    Connections {
        target: AppSettings
        function onAutostartFailed(message) {
            applicationWindow().showPassiveNotification(message);
        }
    }

    Connections {
        target: DataManager
        function onOperationFailed(message) {
            applicationWindow().showPassiveNotification(message);
        }
        function onOperationSucceeded(message) {
            applicationWindow().showPassiveNotification(message);
        }
    }

    Kirigami.PromptDialog {
        id: clearHistoryDialog
        title: i18n("Clear session history?")
        subtitle: i18n("This permanently removes all recorded sessions and statistics. Presets and settings will remain.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Clear History")
                icon.name: "edit-clear-history"
                onTriggered: {
                    DataManager.clearHistory();
                    clearHistoryDialog.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: clearHistoryDialog.close()
            }
        ]
    }

    Kirigami.PromptDialog {
        id: restoreConfirmDialog
        title: i18n("Restore this backup?")
        subtitle: i18n("Restoring replaces your current presets and session history with the contents of the selected backup.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Restore")
                icon.name: "document-open"
                onTriggered: {
                    DataManager.restoreDatabase(restoreDialog.selectedFile);
                    restoreConfirmDialog.close();
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: restoreConfirmDialog.close()
            }
        ]
    }
    readonly property bool wide: width >= Kirigami.Units.gridUnit * 32
    readonly property color cardBorder: Qt.alpha(Kirigami.Theme.textColor, 0.15)

    // Switches bound to a setting lose their binding when the user toggles them. Re-binding
    // afterwards makes them show the real value again, e.g. when a change was refused.
    component SettingSwitch: QQC2.Switch {
        id: sw
        required property string setting
        checked: AppSettings[setting]
        onToggled: {
            AppSettings[setting] = checked;
            checked = Qt.binding(() => AppSettings[sw.setting]);
        }
    }

    component SmallText: QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        lineHeight: 1.0
        color: Kirigami.Theme.disabledTextColor
    }

    /// A titled surface that holds setting rows.
    component SettingsCard: Rectangle {
        id: card
        property string title
        property string iconName
        default property alias rows: rowsColumn.data

        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
        color: Kirigami.Theme.alternateBackgroundColor
        border.width: 1
        border.color: page.cardBorder
        radius: Kirigami.Units.cornerRadius

        ColumnLayout {
            id: cardColumn
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Kirigami.Units.largeSpacing
            }
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Icon {
                    source: card.iconName
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    color: Kirigami.Theme.highlightColor
                    isMask: false
                }
                Kirigami.Heading {
                    level: 2
                    text: card.title
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            ColumnLayout {
                id: rowsColumn
                Layout.fillWidth: true
                spacing: 0
            }
        }
    }

    /// One setting: title and description on the left, controls on the right (stacked when narrow).
    component SettingRow: ColumnLayout {
        id: row
        property string title
        property string description
        property bool first: false
        default property alias controls: controlBox.data

        Layout.fillWidth: true
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: !row.first
            color: page.cardBorder
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing + 2
            Layout.bottomMargin: Kirigami.Units.smallSpacing + 2
            columns: page.wide ? 2 : 1
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: 0
                opacity: row.enabled ? 1 : 0.6
                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.title
                    wrapMode: Text.WordWrap
                }
                SmallText {
                    visible: row.description.length > 0
                    text: row.description
                }
            }

            RowLayout {
                id: controlBox
                Layout.alignment: page.wide ? (Qt.AlignRight | Qt.AlignVCenter) : Qt.AlignLeft
                spacing: Kirigami.Units.smallSpacing
            }
        }
    }


    Item {
        implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2

        ColumnLayout {
            id: content
            width: Math.min(parent.width, Kirigami.Units.gridUnit * 44)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Kirigami.Units.largeSpacing

            SettingsCard {
                title: i18n("Language")
                iconName: "preferences-desktop-locale"
                SettingRow {
                    first: true
                    title: i18n("Language")
                    description: i18n("Choose between the system locale or an explicit language.")
                    QQC2.ComboBox {
                        id: appLanguageBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("System default"), value: "auto" },
                            { text: "English", value: "en" },
                            { text: "Español", value: "es" },
                            { text: "Français", value: "fr" },
                            { text: "Italiano", value: "it" },
                            { text: "Deutsch", value: "de" }
                        ]
                        currentIndex: page.valueIndex(model, AppSettings.language)
                        Accessible.name: i18n("Language")
                        onActivated: AppSettings.language = currentValue
                    }
                }
            }

            SettingsCard {
                title: i18n("Appearance")
                iconName: "preferences-desktop-theme"
                SettingRow {
                    first: true
                    title: i18n("Color scheme")
                    QQC2.ComboBox {
                        id: colorSchemeBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("Breeze Dark"), value: "dark" },
                            { text: i18n("Breeze Light"), value: "light" },
                            { text: i18n("System default"), value: "system" }
                        ]
                        currentIndex: page.valueIndex(model, AppSettings.colorScheme || "dark")
                        Accessible.name: i18n("Color scheme")
                        onActivated: AppSettings.colorScheme = currentValue
                    }
                }
            }

            SettingsCard {
                title: i18n("Startup")
                iconName: "system-run"
                SettingRow {
                    first: true
                    title: i18n("Start kTomato automatically when I log in")
                    description: AppSettings.autostartSupported ? "" : i18n("Starting at login cannot be set up from inside this environment (for example a sandboxed package). Use your desktop's autostart settings instead.")
                    enabled: AppSettings.autostartSupported
                    SettingSwitch { setting: "startAtLogin" }
                }
                SettingRow {
                    title: i18n("Start minimized to the system tray")
                    description: i18n("When started at login, kTomato opens in the tray without showing its window.")
                    enabled: AppSettings.startAtLogin && AppSettings.showTrayIcon && AppSettings.trayAvailable
                    SettingSwitch { setting: "startMinimized" }
                }
            }

            SettingsCard {
                title: i18n("System tray")
                iconName: "preferences-desktop-notification-bell"
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    visible: !AppSettings.trayAvailable
                    type: Kirigami.MessageType.Information
                    text: i18n("No system tray was found on this desktop, so the tray options are not available and closing the window quits kTomato.")
                }
                SettingRow {
                    first: AppSettings.trayAvailable
                    title: i18n("Show an icon in the system tray")
                    description: AppSettings.trayAvailable ? i18n("The tray icon shows the remaining time. Click it to show or hide the window; use its menu to start, pause, stop or quit.") : ""
                    enabled: AppSettings.trayAvailable
                    SettingSwitch { setting: "showTrayIcon" }
                }
                SettingRow {
                    title: i18n("Keep running in the tray when the window is closed")
                    enabled: AppSettings.showTrayIcon && AppSettings.trayAvailable
                    SettingSwitch { setting: "closeToTray" }
                }
                SettingRow {
                    title: i18n("Badge style")
                    enabled: AppSettings.showTrayIcon && AppSettings.trayAvailable
                    QQC2.ComboBox {
                        id: trayBadgeBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("Remaining minutes"), value: 0 },
                            { text: i18n("Progress pie ring"), value: 1 },
                            { text: i18n("Plain icon"), value: 2 }
                        ]
                        currentIndex: AppSettings.trayBadgeStyle
                        Accessible.name: i18n("Badge style")
                        onActivated: AppSettings.trayBadgeStyle = currentValue
                    }
                }
            }

            SettingsCard {
                title: i18n("Sounds and Notifications")
                iconName: "audio-volume-high"
                SettingRow {
                    first: true
                    title: i18n("Phase sound")
                    QQC2.ComboBox {
                        id: soundBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("Alarm Clock (Default)"), value: "alarm-clock-elapsed" },
                            { text: i18n("Completion Chime"), value: "complete" },
                            { text: i18n("Instant Message"), value: "message-new-instant" },
                            { text: i18n("Information Sound"), value: "dialog-information" },
                            { text: i18n("Bell"), value: "bell" }
                        ]
                        currentIndex: page.valueIndex(model, AppSettings.soundTheme)
                        Accessible.name: i18n("Phase sound")
                        onActivated: AppSettings.soundTheme = currentValue
                    }
                    QQC2.Button {
                        text: i18n("Test")
                        icon.name: "media-playback-start"
                        onClicked: AppSettings.playSoundPreview(soundBox.currentValue, AppSettings.soundVolume)
                    }
                }
                SettingRow {
                    title: i18n("Alert volume")
                    QQC2.Slider {
                        id: soundVolumeSlider
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                        from: 0
                        to: 100
                        value: AppSettings.soundVolume
                        Accessible.name: i18n("Alert volume")
                        onMoved: AppSettings.soundVolume = Math.round(value)
                    }
                    QQC2.Label {
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                        horizontalAlignment: Text.AlignRight
                        text: i18nc("volume percentage", "%1%", Math.round(soundVolumeSlider.value))
                    }
                }
                SettingRow {
                    title: i18n("Play a warning one minute before a phase ends")
                    SettingSwitch { setting: "preAlarmEnabled" }
                }
                SettingRow {
                    title: i18n("Pre-alarm sound")
                    enabled: AppSettings.preAlarmEnabled
                    QQC2.ComboBox {
                        id: preAlarmBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("Information Sound"), value: "dialog-information" },
                            { text: i18n("Bell"), value: "bell" },
                            { text: i18n("Completion Chime"), value: "complete" },
                            { text: i18n("Alarm Clock"), value: "alarm-clock-elapsed" }
                        ]
                        currentIndex: page.valueIndex(model, AppSettings.preAlarmSound)
                        Accessible.name: i18n("Pre-alarm sound")
                        onActivated: AppSettings.preAlarmSound = currentValue
                    }
                    QQC2.Button {
                        text: i18n("Test")
                        enabled: AppSettings.preAlarmEnabled
                        icon.name: "media-playback-start"
                        onClicked: AppSettings.playSoundPreview(preAlarmBox.currentValue, AppSettings.soundVolume)
                    }
                }
            }

            SettingsCard {
                title: i18n("Focus sound")
                iconName: "headphones"
                SettingRow {
                    first: true
                    title: i18n("Focus sound")
                    QQC2.ComboBox {
                        id: focusModeBox
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18n("Off"), value: 0 },
                            { text: i18n("Clock tick"), value: 1 },
                            { text: i18n("Rain"), value: 2 },
                            { text: i18n("White noise"), value: 3 }
                        ]
                        currentIndex: AppSettings.focusSoundMode
                        Accessible.name: i18n("Focus sound")
                        onActivated: AppSettings.focusSoundMode = currentValue
                    }
                }
                SettingRow {
                    title: i18n("Focus sound volume")
                    enabled: AppSettings.focusSoundMode > 0
                    QQC2.Slider {
                        id: focusVolumeSlider
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        from: 0
                        to: 100
                        value: AppSettings.focusSoundVolume
                        Accessible.name: i18n("Focus sound volume")
                        onMoved: AppSettings.focusSoundVolume = Math.round(value)
                    }
                    QQC2.Label {
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                        horizontalAlignment: Text.AlignRight
                        text: i18nc("volume percentage", "%1%", Math.round(focusVolumeSlider.value))
                    }
                    QQC2.Button {
                        text: i18n("Preview")
                        icon.name: "media-playback-start"
                        onClicked: FocusSoundController.preview(focusModeBox.currentValue, Math.round(focusVolumeSlider.value))
                    }
                }
            }

            SettingsCard {
                title: i18n("Screen")
                iconName: "preferences-system-power-management"
                SettingRow {
                    first: true
                    title: i18n("Keep the screen awake during work sessions")
                    SettingSwitch { setting: "keepScreenAwake" }
                }
                SettingRow {
                    title: i18n("Pause timer when screen is locked")
                    description: i18n("Automatically pauses the active work phase when you lock your screen and prompts to resume when you unlock.")
                    SettingSwitch { setting: "autoPauseOnScreenLock" }
                }
            }

            SettingsCard {
                title: i18n("Productivity and Goals")
                iconName: "games-highscores"
                SettingRow {
                    first: true
                    title: i18n("Daily target")
                    description: i18n("Your daily target is shown as an indicator on the timer page and on the statistics dashboard.")
                    // 0 disables the daily goal. The text carries the unit, so any text is accepted
                    // while typing and the number is read out of it.
                    QQC2.SpinBox {
                        id: goalBox
                        Accessible.name: i18n("Daily target")
                        from: 0
                        to: 50
                        editable: true
                        value: AppSettings.dailyGoal
                        validator: RegularExpressionValidator { regularExpression: /.*/ }
                        textFromValue: (value, locale) => value === 0
                            ? i18n("Disabled")
                            : i18np("%1 pomodoro / day", "%1 pomodoros / day", value)
                        valueFromText: (text, locale) => {
                            const match = text.match(/\d+/);
                            if (match) {
                                return parseInt(match[0]);
                            }
                            return text.trim() === i18n("Disabled") ? 0 : goalBox.value;
                        }
                        onValueModified: AppSettings.dailyGoal = value
                    }
                }
                SettingRow {
                    title: i18n("Protect daily streak over weekends")
                    description: i18n("When enabled, weekends without recorded sessions do not reset your daily streak on Monday.")
                    SettingSwitch { setting: "protectWeekendStreak" }
                }
                SettingRow {
                    title: i18n("Prompt for a task note when a work session finishes")
                    description: i18n("Sends a notification allowing you to record what you worked on. Notes are saved to session history and exported with CSV reports.")
                    SettingSwitch { setting: "promptTaskNote" }
                }
            }

            SettingsCard {
                id: calendarCard
                title: i18n("Calendar")
                iconName: "view-calendar"

                SettingRow {
                    first: true
                    title: i18n("Add an event to your calendar each time you finish a work session.")
                    QQC2.Switch {
                        id: calendarSwitch
                        checked: page.calOn
                        Accessible.name: i18n("Calendar")
                        onToggled: {
                            if (CalendarSync) {
                                CalendarSync.enabled = checked;
                            }
                            checked = Qt.binding(() => page.calOn);
                        }
                    }
                }

                ColumnLayout {
                    id: calendarBody
                    Layout.fillWidth: true
                    spacing: 0
                    enabled: page.calOn
                    opacity: page.calOn ? 1 : 0.6

                    SettingRow {
                        title: i18n("Calendar service")
                        QQC2.ComboBox {
                            id: calProviderBox
                            textRole: "text"
                            valueRole: "value"
                            model: [
                                { text: i18n("Nextcloud"), value: "nextcloud" },
                                { text: i18n("Custom CalDAV"), value: "caldav" }
                            ]
                            currentIndex: page.valueIndex(model, page.calProvider)
                            Accessible.name: i18n("Calendar service")
                            onActivated: {
                                if (CalendarSync) {
                                    CalendarSync.provider = currentValue;
                                    // Each service keeps its own settings: if the one just chosen is
                                    // already signed in, load its calendars.
                                    if (CalendarSync.hasPassword) {
                                        CalendarSync.refreshCalendars();
                                    }
                                }
                                currentIndex = Qt.binding(() => page.valueIndex(model, page.calProvider));
                            }
                        }
                    }

                    // Nextcloud: server address and browser login.
                    SettingRow {
                        visible: !page.calCustom
                        title: i18n("Server address")
                        enabled: !page.calConnected && !page.calWaiting
                        QQC2.TextField {
                            Layout.fillWidth: !page.wide
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                            placeholderText: "https://cloud.example.org"
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                            text: CalendarSync ? CalendarSync.serverUrl : ""
                            Accessible.name: i18n("Server address")
                            onEditingFinished: {
                                if (CalendarSync) {
                                    CalendarSync.serverUrl = text;
                                }
                                text = Qt.binding(() => CalendarSync ? CalendarSync.serverUrl : "");
                            }
                        }
                    }
                    SettingRow {
                        visible: !page.calCustom
                        title: page.calConnected
                            ? i18n("Connected as %1", CalendarSync ? CalendarSync.username : "")
                            : (page.calWaiting && CalendarSync ? CalendarSync.statusText : "")
                        QQC2.BusyIndicator {
                            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                            visible: page.calWaiting
                            running: visible && page.visible
                        }
                        QQC2.Button {
                            visible: !page.calConnected && !page.calWaiting
                            text: i18n("Log in")
                            icon.name: "network-connect"
                            onClicked: CalendarSync.startNextcloudLogin()
                        }
                        QQC2.Button {
                            visible: page.calWaiting
                            text: i18n("Cancel")
                            icon.name: "dialog-cancel"
                            onClicked: CalendarSync.cancelLogin()
                        }
                        QQC2.Button {
                            visible: page.calConnected && !page.calWaiting
                            text: i18n("Test")
                            icon.name: page.calTestOk ? "dialog-ok-apply" : "network-wireless-connected-100"
                            icon.color: page.calTestOk ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.textColor
                            enabled: !page.calTesting
                            Accessible.name: i18n("Test connection")
                            QQC2.ToolTip.text: i18n("Test connection")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                page.calAction = "test";
                                CalendarSync.testConnection();
                            }
                        }
                        QQC2.Button {
                            visible: page.calConnected && !page.calWaiting
                            text: i18n("Disconnect")
                            icon.name: "network-disconnect"
                            onClicked: CalendarSync.disconnect()
                        }
                    }

                    // Custom CalDAV: address, username, password.
                    SettingRow {
                        visible: page.calCustom
                        title: i18n("Calendar address (CalDAV)")
                        description: i18n("Use the CalDAV address of the calendar. A subscription link ending in .ics is read-only and cannot receive events.")
                        QQC2.TextField {
                            Layout.fillWidth: !page.wide
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                            text: CalendarSync ? CalendarSync.calendarUrl : ""
                            Accessible.name: i18n("Calendar address (CalDAV)")
                            onEditingFinished: {
                                if (CalendarSync) {
                                    CalendarSync.calendarUrl = text;
                                }
                                text = Qt.binding(() => CalendarSync ? CalendarSync.calendarUrl : "");
                            }
                        }
                    }
                    SettingRow {
                        visible: page.calCustom
                        title: i18n("Username")
                        QQC2.TextField {
                            Layout.fillWidth: !page.wide
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                            text: CalendarSync ? CalendarSync.username : ""
                            Accessible.name: i18n("Username")
                            onEditingFinished: {
                                if (CalendarSync) {
                                    CalendarSync.username = text;
                                }
                                text = Qt.binding(() => CalendarSync ? CalendarSync.username : "");
                            }
                        }
                    }
                    SettingRow {
                        visible: page.calCustom
                        title: i18n("Password")
                        QQC2.TextField {
                            id: calPasswordField
                            property bool dirty: false
                            Layout.fillWidth: !page.wide
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                            echoMode: TextInput.Password
                            // The saved password is never read back; dots only show that one is stored.
                            placeholderText: page.calHasPassword ? "••••••••" : ""
                            Accessible.name: i18n("Password")
                            onTextEdited: dirty = true
                            onEditingFinished: {
                                if (dirty && CalendarSync) {
                                    CalendarSync.setPassword(text);
                                }
                                dirty = false;
                                text = "";
                            }
                        }
                    }
                    // Same row as for Nextcloud: test the connection, or forget this service's login.
                    SettingRow {
                        visible: page.calCustom && page.calConfigured
                        title: ""
                        QQC2.Button {
                            text: i18n("Test")
                            icon.name: page.calTestOk ? "dialog-ok-apply" : "network-wireless-connected-100"
                            icon.color: page.calTestOk ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.textColor
                            enabled: !page.calTesting
                            Accessible.name: i18n("Test connection")
                            QQC2.ToolTip.text: i18n("Test connection")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                page.calAction = "test";
                                CalendarSync.testConnection();
                            }
                        }
                        QQC2.Button {
                            text: i18n("Disconnect")
                            icon.name: "network-disconnect"
                            onClicked: CalendarSync.disconnect()
                        }
                    }
                    // Calendar choice, with a refresh button to reload the list from the server.
                    SettingRow {
                        visible: page.calChoices.length > 0 || page.calConfigured
                        title: i18n("Calendar")
                        QQC2.ComboBox {
                            id: calChoiceBox
                            Layout.fillWidth: !page.wide
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                            textRole: "name"
                            valueRole: "url"
                            model: page.calChoices
                            currentIndex: page.calChoiceIndex()
                            Accessible.name: i18n("Calendar")
                            onActivated: {
                                const item = page.calChoices[currentIndex];
                                if (CalendarSync && item) {
                                    CalendarSync.selectCalendar(item.url, item.name);
                                }
                                currentIndex = Qt.binding(() => page.calChoiceIndex());
                            }
                            enabled: page.calChoices.length > 0
                            delegate: QQC2.ItemDelegate {
                                required property var modelData
                                required property int index
                                width: calChoiceBox.width
                                highlighted: calChoiceBox.highlightedIndex === index
                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.smallSpacing
                                    Rectangle {
                                        visible: !!modelData.color
                                        implicitWidth: Kirigami.Units.gridUnit * 0.6
                                        implicitHeight: implicitWidth
                                        radius: width / 2
                                        color: modelData.color || "transparent"
                                        border.width: 1
                                        border.color: page.cardBorder
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        elide: Text.ElideRight
                                        color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                    }
                                }
                            }
                        }
                        QQC2.ToolButton {
                            icon.name: "view-refresh"
                            display: QQC2.AbstractButton.IconOnly
                            enabled: !page.calRefreshing
                            Accessible.name: i18n("Find calendars")
                            QQC2.ToolTip.text: i18n("Find calendars")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                page.calAction = "refresh";
                                CalendarSync.refreshCalendars();
                            }
                        }
                    }

                    SettingRow {
                        title: i18n("Include the task note in the event description")
                        description: i18n("Notes are sent to your calendar server.")
                        QQC2.Switch {
                            checked: CalendarSync ? CalendarSync.includeNote : false
                            Accessible.name: i18n("Include the task note in the event description")
                            onToggled: {
                                if (CalendarSync) {
                                    CalendarSync.includeNote = checked;
                                }
                                checked = Qt.binding(() => CalendarSync ? CalendarSync.includeNote : false);
                            }
                        }
                    }
                }

                // Status area, kept readable even while the sync is switched off. Nothing is shown while
                // the server is being asked (the buttons are disabled meanwhile): only a result is.
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    visible: CalendarSync !== null && (page.calStatus === CalendarSync.Failed
                        || (page.calStatus === CalendarSync.NotConfigured
                            && !page.calCustom && !page.calConnected && !page.calWaiting))
                    type: page.calStatus === CalendarSync.Failed ? Kirigami.MessageType.Error
                        : Kirigami.MessageType.Information
                    text: {
                        if (!CalendarSync) {
                            return "";
                        }
                        if (page.calStatus === CalendarSync.NotConfigured) {
                            return i18n("Choose a calendar service and sign in to start sending events.");
                        }
                        if (page.calStatus === CalendarSync.Failed && CalendarSync.lastError.length > 0) {
                            return CalendarSync.lastError;
                        }
                        return CalendarSync.statusText;
                    }
                }
                SmallText {
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    visible: page.calLastSuccess.length > 0
                    text: i18n("Last event sent: %1", page.calLastSuccess)
                }
                SmallText {
                    visible: page.calPending > 0
                    text: i18np("%1 event waiting to be sent", "%1 events waiting to be sent", page.calPending)
                }
                SmallText {
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    text: i18n("When enabled, kTomato sends the name of your timer, its category and the time of each finished work session to the calendar server you choose.")
                }
            }

            SettingsCard {
                title: i18n("Data management")
                iconName: "drive-harddisk"
                SettingRow {
                    first: true
                    title: i18n("Database: %1", DataManager ? DataManager.databasePath : "")
                    description: TimerEngine.active ? i18n("Stop the active timer before restoring a backup.") : ""
                    QQC2.Button {
                        text: i18n("Back up…")
                        icon.name: "document-save"
                        onClicked: backupDialog.open()
                    }
                    QQC2.Button {
                        text: i18n("Restore…")
                        icon.name: "document-open"
                        enabled: !TimerEngine.active
                        onClicked: restoreDialog.open()
                    }
                }
                SettingRow {
                    title: i18n("Session history")
                    QQC2.Button {
                        id: clearHistoryButton
                        text: i18n("Clear history…")
                        icon.name: "edit-clear-history"
                        icon.color: Kirigami.Theme.negativeTextColor
                        palette.buttonText: Kirigami.Theme.negativeTextColor
                        enabled: DataManager !== null && DataManager.sessionCount > 0
                        onClicked: clearHistoryDialog.open()
                    }
                }
            }

            SettingsCard {
                title: i18n("Global Shortcuts and Automation")
                iconName: "utilities-terminal"
                SettingRow {
                    first: true
                    title: i18n("D-Bus API:")
                    description: i18n("kTomato exposes a native D-Bus interface at %1. You can bind global keys in KDE System Settings → Shortcuts to a command like the one below.",
                                      "io.github.mineraleyt.ktomato /Timer")
                    Item { implicitWidth: 1 }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: dbusLabel.implicitHeight + Kirigami.Units.largeSpacing
                    color: Kirigami.Theme.backgroundColor
                    border.width: 1
                    border.color: page.cardBorder
                    radius: Kirigami.Units.cornerRadius
                    Kirigami.SelectableLabel {
                        id: dbusLabel
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        text: "qdbus6 io.github.mineraleyt.ktomato /Timer toggle"
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                        Accessible.name: text
                    }
                }
                SettingRow {
                    title: i18n("Open Global Shortcuts")
                    description: i18n("Available commands: %1.", "toggle, start, pause, stop, skip, status")
                    QQC2.Button {
                        text: i18n("Open Global Shortcuts")
                        icon.name: "configure-shortcuts"
                        onClicked: AppSettings.openShortcutsSettings()
                    }
                }
            }

            SettingsCard {
                title: i18n("Troubleshooting & Diagnostics")
                iconName: "tools-report-bug"
                SettingRow {
                    first: true
                    title: i18n("Diagnostics:")
                    description: i18n("Generate a redacted report with system information and recent logs to share when reporting an issue.")
                    Item { implicitWidth: 1 }
                }
                Flow {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Button {
                        text: i18n("Copy to Clipboard")
                        icon.name: "edit-copy"
                        onClicked: {
                            Diagnostics.copyToClipboard();
                            page.showDiagnosticsResult("copied");
                        }
                    }
                    QQC2.Button {
                        text: i18n("Export Diagnostics…")
                        icon.name: "document-export"
                        onClicked: exportDiagnosticsDialog.open()
                    }
                    QQC2.Button {
                        text: i18n("View Report…")
                        icon.name: "document-preview"
                        onClicked: {
                            diagnosticsPreviewDialog.reportText = Diagnostics.generateReport();
                            diagnosticsPreviewDialog.open();
                        }
                    }
                }
        Kirigami.InlineMessage {
            id: diagnosticsInlineMessage
            readonly property bool shouldShow: page.diagnosticsResult.length > 0 && !page.diagnosticsDismissed
            Layout.fillWidth: true
            visible: shouldShow
            showCloseButton: true
            type: page.diagnosticsResult === "exportFailed" ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            text: {
                switch (page.diagnosticsResult) {
                case "copied":
                    return i18n("Diagnostic report copied to clipboard.");
                case "exported":
                    return i18n("Diagnostic report successfully exported.");
                case "exportFailed":
                    return page.diagnosticsExportError.length > 0
                        ? page.diagnosticsExportError
                        : i18n("Failed to export diagnostic report.");
                }
                return "";
            }
            // The close button assigns visible = false, which removes the binding: remember the
            // dismissal and bind again, so the next result is shown.
            onVisibleChanged: {
                if (!visible && shouldShow) {
                    page.diagnosticsDismissed = true;
                    visible = Qt.binding(() => diagnosticsInlineMessage.shouldShow);
                }
            }
        }

            }
        }
    }

        FileDialog {
            id: backupDialog
            title: i18n("Back up kTomato database")
            fileMode: FileDialog.SaveFile
            defaultSuffix: "db"
            nameFilters: [i18n("SQLite database (*.db)")]
            onAccepted: DataManager.backupDatabase(selectedFile)
        }

        FileDialog {
            id: restoreDialog
            title: i18n("Choose a kTomato backup")
            fileMode: FileDialog.OpenFile
            nameFilters: [i18n("SQLite database (*.db)")]
            onAccepted: restoreConfirmDialog.open()
        }

        FileDialog {
            id: exportDiagnosticsDialog
            title: i18n("Export Diagnostics")
            fileMode: FileDialog.SaveFile
            defaultSuffix: "txt"
            nameFilters: [i18n("Text files (*.txt)"), i18n("All files (*)")]
            onAccepted: {
                if (Diagnostics.exportReport(selectedFile)) {
                    page.showDiagnosticsResult("exported");
                } else {
                    page.diagnosticsExportError = Diagnostics.lastExportError();
                    page.showDiagnosticsResult("exportFailed");
                }
            }
        }

        Kirigami.PromptDialog {
            id: diagnosticsPreviewDialog
            title: i18n("Diagnostics Report")
            subtitle: i18n("All personal details and usernames have been redacted.")
            preferredWidth: Kirigami.Units.gridUnit * 36
            preferredHeight: Kirigami.Units.gridUnit * 28
            standardButtons: Kirigami.Dialog.Close
            property string reportText: ""

            // A child (not contentItem): PromptDialog draws its title and subtitle inside its
            // content, and replacing contentItem would remove them. The dialog scrolls itself.
            QQC2.TextArea {
                Layout.fillWidth: true
                readOnly: true
                text: diagnosticsPreviewDialog.reportText
                font.family: "monospace"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                wrapMode: Text.Wrap
                Accessible.name: diagnosticsPreviewDialog.title
            }

            customFooterActions: [
                Kirigami.Action {
                    text: i18n("Copy to Clipboard")
                    icon.name: "edit-copy"
                    onTriggered: {
                        Diagnostics.copyToClipboard();
                        diagnosticsPreviewDialog.close();
                        page.showDiagnosticsResult("copied");
                    }
                }
            ]
        }
}
