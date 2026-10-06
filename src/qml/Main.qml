// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.ApplicationWindow {
    id: root

    title: {
        if (!TimerEngine.active) {
            return i18n("kTomato");
        }
        const m = Math.floor(TimerEngine.remainingSeconds / 60);
        const s = TimerEngine.remainingSeconds % 60;
        const pad = n => String(n).padStart(2, "0");
        const timeStr = pad(m) + ":" + pad(s);
        let phaseStr = i18n("Work");
        if (TimerEngine.phase === TimerEngine.Phase.ShortBreak) {
            phaseStr = i18n("Short break");
        } else if (TimerEngine.phase === TimerEngine.Phase.LongBreak) {
            phaseStr = i18n("Long break");
        }
        if (TimerEngine.paused) {
            return i18n("[%1] %2 (Paused) — kTomato", timeStr, phaseStr);
        }
        return i18n("[%1] %2 — kTomato", timeStr, phaseStr);
    }
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 36
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 28

    // Started with --background and a tray icon is there: begin hidden. The initial visibility
    // is set once in Component.onCompleted: a binding would hide the window again whenever the
    // tray or start-minimized settings change later.

    // The close button hides the window when the program is meant to live in the tray;
    // otherwise it quits (the application does not quit on its own when a window closes).
    onClosing: close => {
        if (AppSettings.keepRunningInTray) {
            close.accepted = false;
            root.hide();
        } else {
            Qt.quit();
        }
    }

    /// Asks what was done during a finished work session. `sessionId` is the session the note
    /// is stored on; without one the most recent work session is used.
    function openTaskNotePrompt(presetName, sessionId) {
        let id = (sessionId === undefined || sessionId === null) ? DataManager.lastWorkSessionId() : Number(sessionId);
        if (!(id > 0)) {
            return;
        }
        taskNoteDialog.presetName = presetName || "";
        taskNoteDialog.sessionId = id;
        taskNoteDialog.open();
    }

    function saveTaskNote(note: string): void {
        const text = note.trim();
        if (text.length > 0 && taskNoteDialog.sessionId > 0) {
            DataManager.updateSessionNote(taskNoteDialog.sessionId, text);
        }
        taskNoteDialog.close();
    }

    // Taskbar flash and optional in-app note prompt on phase completion.
    Connections {
        target: TimerEngine
        function onPhaseFinished(finished, next) {
            root.alert(0);
            if (finished === TimerEngine.Phase.Work && AppSettings.promptTaskNote && root.visible && root.active) {
                root.openTaskNotePrompt(PresetModel.currentName, DataManager.lastWorkSessionId());
            }
        }
    }

    // --- Page creation -------------------------------------------------------
    //
    // Passing a URL to pageStack.push() makes Kirigami's PageRow instantiate the page with a
    // non-visual parent, which makes Qt print "Created graphical object was not placed in the
    // graphics scene" for every page. Pages are therefore created here under a visual parent;
    // the page row reparents them when they are pushed. The row does not own such pages, so
    // pageRemoved() destroys them.

    Item {
        id: pageParking
        visible: false
    }

    property var pageComponents: ({})
    Timer {
        interval: 100
        repeat: true
        running: appStatsSmokeTest
        property int attempts: 0
        onTriggered: {
            ++attempts;
            const statsPage = root.pageStack.currentItem;
            if (statsPage && statsPage.title === i18n("Statistics") && statsPage.contentLaidOut) {
                Qt.exit(0);
            } else if (attempts >= 50) {
                const component = root.pageComponents["StatsPage.qml"];
                console.error("Statistics page did not lay out within five seconds:",
                              "destination=", root.currentDestination,
                              "component=", component ? component.status : "missing",
                              "error=", component ? component.errorString() : "none",
                              "currentPage=", statsPage ? statsPage.title : "missing",
                              "contentReady=", statsPage ? statsPage.contentLaidOut : "unavailable");
                if (root.currentDestination !== "stats") {
                    Qt.exit(11);
                } else if (!component) {
                    Qt.exit(12);
                } else if (component.status === Component.Loading) {
                    Qt.exit(13);
                } else if (component.status === Component.Error) {
                    const error = component.errorString();
                    Qt.exit(error.length > 0 ? error.charCodeAt(0) : 200);
                } else if (!statsPage) {
                    Qt.exit(15);
                } else {
                    Qt.exit(16);
                }
            }
        }
    }

    Timer {
        interval: 100
        repeat: true
        running: typeof appSmokePage !== "undefined" && appSmokePage.length > 0
        property int attempts: 0
        property bool subPageOpened: false
        onTriggered: {
            ++attempts;
            const currentPage = root.pageStack.currentItem;
            if (currentPage && attempts >= 3 && appSmokePage === "presets" && !subPageOpened) {
                // Choosing the current destination again must leave an open sub-page.
                subPageOpened = true;
                root.pushPage("PresetEditPage.qml", { uuid: "" });
                if (root.pageStack.depth !== 2) {
                    console.error("Timer editor did not open: depth=", root.pageStack.depth);
                    Qt.exit(21);
                    return;
                }
                root.navigate("presets");
                if (root.pageStack.depth !== 1) {
                    console.error("Navigating to the current page did not close its sub-page: depth=",
                                  root.pageStack.depth);
                    Qt.exit(22);
                }
            } else if (currentPage && attempts >= 3) {
                Qt.exit(0);
            } else if (attempts >= 50) {
                const targetFile = root.destinations[appSmokePage];
                const component = targetFile ? root.pageComponents[targetFile] : null;
                console.error("Page", appSmokePage, "did not load within five seconds:",
                              "component=", component ? component.status : "missing",
                              "error=", component ? component.errorString() : "none",
                              "currentPage=", currentPage ? currentPage.title : "missing");
                Qt.exit(20);
            }
        }
    }

    /// `file` is relative to the pages/ directory, e.g. "TimerPage.qml".
    function createPage(file: string, properties: var): Item {
        let component = pageComponents[file];
        if (!component) {
            component = Qt.createComponent(Qt.resolvedUrl("pages/" + file));
            pageComponents[file] = component;
        }
        if (component.status !== Component.Ready) {
            console.error("Cannot load page", file + ":", component.errorString());
            return null;
        }
        return component.createObject(pageParking, properties ?? {});
    }

    /// Creates the page and pushes it; pages call this to open sub-pages.
    function pushPage(file: string, properties: var): Item {
        const page = createPage(file, properties);
        if (page) {
            pageStack.push(page);
        }
        return page;
    }

    Connections {
        target: root.pageStack
        function onPageRemoved(page) {
            page.destroy();
        }
    }

    // --- Navigation ----------------------------------------------------------

    readonly property var destinations: ({
        timer: "TimerPage.qml",
        presets: "PresetsPage.qml",
        stats: "StatsPage.qml",
        settings: "AppSettingsPage.qml",
        about: "AboutPage.qml",
        welcome: "WelcomePage.qml"
    })

    // Name of the visible top-level destination: "timer", "presets", "stats", "settings", "about" or "welcome".
    property string currentDestination: "timer"
    readonly property bool isWelcomeActive: currentDestination === "welcome"

    // Top-level destinations replace each other instead of stacking.
    function navigate(name: string): void {
        if (currentDestination === name) {
            // Already there, possibly with sub-pages (e.g. a timer editor) on top: go back to the
            // top-level page.
            if (pageStack.depth > 1) {
                pageStack.pop(pageStack.items[0]);
            }
            return;
        }
        currentDestination = name;
        pageStack.clear();
        pushPage(destinations[name]);
        if (name === "welcome") {
            navDrawer.drawerOpen = false;
        } else if (root.wideScreen) {
            navDrawer.drawerOpen = true;
        }
    }

    onIsWelcomeActiveChanged: {
        if (isWelcomeActive) {
            navDrawer.drawerOpen = false;
        } else if (root.wideScreen) {
            navDrawer.drawerOpen = true;
        }
    }

    onWideScreenChanged: {
        if (!isWelcomeActive && root.wideScreen) {
            navDrawer.drawerOpen = true;
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: Qt.quit()
    }

    // True while a control that reacts to Space itself (text inputs, buttons, combo boxes)
    // has keyboard focus: the key then belongs to it.
    readonly property bool spaceConsumedByFocus: activeFocusItem !== null
                                                 && (activeFocusItem.hasOwnProperty("cursorPosition")
                                                     || activeFocusItem.hasOwnProperty("checkable")
                                                     || activeFocusItem.hasOwnProperty("popup"))
    readonly property bool dialogOpen: taskNoteDialog.visible || screenLockResumeDialog.visible

    // Space starts, pauses or resumes the timer while the timer page is shown.
    Shortcut {
        sequence: "Space"
        enabled: root.currentDestination === "timer" && root.pageStack.depth === 1
                 && !root.spaceConsumedByFocus && !root.dialogOpen
        onActivated: TimerEngine.toggle()
    }

    Repeater {
        model: ["timer", "presets", "stats", "settings", "about"]
        delegate: Item {
            id: pageShortcut
            required property string modelData
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (pageShortcut.index + 1)
                enabled: !root.isWelcomeActive && !root.dialogOpen
                onActivated: root.navigate(pageShortcut.modelData)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Preferences, "Ctrl+,"]
        enabled: !root.isWelcomeActive && !root.dialogOpen
        onActivated: root.navigate("settings")
    }

    // Shown on every page: nothing recorded now will survive a restart.
    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        visible: DataManager !== null && DataManager.usingTemporaryDatabase === true
        type: Kirigami.MessageType.Warning
        text: i18n("Your data cannot be saved: the database could not be opened. kTomato is using a temporary database.")
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: navDrawer
        isMenu: false
        collapsible: false
        collapseButtonVisible: false
        // Narrow windows use a modal drawer: the handle puts a menu button in the toolbar.
        handleVisible: !root.wideScreen && !root.isWelcomeActive
        modal: !root.wideScreen
        drawerOpen: false

        topContent: [
            ColumnLayout {
                id: navColumn
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                spacing: Kirigami.Units.smallSpacing / 2
                Layout.topMargin: Kirigami.Units.mediumSpacing
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing

                // Branding header.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        source: "io.github.mineraleyt.ktomato"
                        fallback: "chronometer"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.large
                        Layout.preferredHeight: Kirigami.Units.iconSizes.large
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Kirigami.Heading {
                            level: 3
                            type: Kirigami.Heading.Type.Primary
                            text: i18n("kTomato")
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            readonly property var about: typeof appAboutData !== "undefined" ? appAboutData : null
                            visible: about !== null && about.version.length > 0
                            text: about ? i18n("Version %1", about.version) : ""
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }

                Repeater {
                    model: [
                        { id: "timer", name: i18n("Timer"), icon: "chronometer" },
                        { id: "presets", name: i18n("Timers"), icon: "view-list-details" },
                        { id: "stats", name: i18n("Statistics"), icon: "office-chart-bar" },
                        { id: "settings", name: i18n("Settings"), icon: "configure", separator: true },
                        { id: "about", name: i18n("About"), icon: "help-about" }
                    ]

                    delegate: ColumnLayout {
                        id: navEntry
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing / 2

                        Kirigami.Separator {
                            visible: navEntry.modelData.separator === true
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                        }

                    QQC2.ItemDelegate {
                        id: navItem
                        property var modelData: navEntry.modelData

                        readonly property bool isCurrent: root.currentDestination === modelData.id

                        Layout.fillWidth: true
                        implicitHeight: Kirigami.Units.gridUnit * 2.3
                        // Not drawn (custom contentItem), but read by screen readers.
                        text: modelData.name
                        Accessible.name: modelData.name

                        onClicked: {
                            root.navigate(modelData.id);
                            if (navDrawer.modal) {
                                navDrawer.drawerOpen = false;
                            }
                        }

                        background: Rectangle {
                            radius: Kirigami.Units.cornerRadius
                            border.width: navItem.visualFocus ? 1 : 0
                            border.color: Kirigami.Theme.highlightColor
                            Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
                            color: navItem.isCurrent
                                   ? Qt.alpha(Kirigami.Theme.highlightColor, navItem.pressed ? 0.28 : 0.16)
                                   : (navItem.pressed
                                      ? Qt.alpha(Kirigami.Theme.textColor, 0.12)
                                      : (navItem.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.06) : "transparent"))

                            Rectangle {
                                visible: navItem.isCurrent
                                anchors {
                                    left: parent.left
                                    top: parent.top
                                    bottom: parent.bottom
                                    topMargin: Kirigami.Units.smallSpacing
                                    bottomMargin: Kirigami.Units.smallSpacing
                                    leftMargin: 2
                                }
                                width: 3
                                radius: 1.5
                                color: Kirigami.Theme.highlightColor
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.mediumSpacing

                            Kirigami.Icon {
                                source: (navItem.modelData.id === "about" && UpdateChecker.hasUpdate)
                                    ? "update-low"
                                    : navItem.modelData.icon
                                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                                color: (navItem.modelData.id === "about" && UpdateChecker.hasUpdate)
                                    ? Kirigami.Theme.highlightColor
                                    : (navItem.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor)
                            }

                            QQC2.Label {
                                text: navItem.modelData.name
                                font.weight: navItem.isCurrent ? Font.DemiBold : Font.Normal
                                color: navItem.isCurrent ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Rectangle {
                                visible: navItem.modelData.id === "about" && UpdateChecker.hasUpdate
                                implicitWidth: Kirigami.Units.gridUnit * 1.3
                                implicitHeight: Kirigami.Units.gridUnit * 1.3
                                radius: width / 2
                                color: "transparent"
                                border.width: 1.5
                                border.color: Kirigami.Theme.highlightColor
                                Layout.alignment: Qt.AlignVCenter
                                Layout.rightMargin: Kirigami.Units.smallSpacing

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: "1"
                                    font.bold: true
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: Kirigami.Theme.highlightColor
                                }
                            }
                        }
                    }
                    }
                }
            }
        ]
    }

    Component.onCompleted: {
        root.visible = !AppSettings.startHidden;
        if (!AppSettings.firstRunCompleted && !appStatsSmokeTest && (!appSmokePage || appSmokePage.length === 0)) {
            currentDestination = "welcome";
            navDrawer.drawerOpen = false;
            pushPage(destinations.welcome);
        } else {
            if (typeof appSmokePage !== "undefined" && appSmokePage.length > 0) {
                currentDestination = appSmokePage;
            } else if (appStatsSmokeTest) {
                currentDestination = "stats";
            }
            navDrawer.drawerOpen = root.wideScreen;
            pushPage(destinations[currentDestination]);
        }
    }

    // Single-column navigation: top-level pages never sit side by side.
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn

    Kirigami.PromptDialog {
        id: taskNoteDialog
        property string presetName: ""
        property real sessionId: -1
        preferredWidth: Kirigami.Units.gridUnit * 26
        title: i18n("Work Session Finished")
        subtitle: presetName.length > 0
            ? i18n("What did you accomplish during “%1”?", presetName)
            : i18n("What did you accomplish during this session?")

        standardButtons: Kirigami.Dialog.NoButton

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            QQC2.TextField {
                id: taskNoteField
                Layout.fillWidth: true
                placeholderText: i18n("e.g. Refactored auth module, wrote tests…")
                onAccepted: root.saveTaskNote(text)
            }

            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                visible: recentRepeater.count > 0

                Repeater {
                    id: recentRepeater
                    model: []

                    QQC2.Button {
                        required property string modelData
                        text: modelData
                        icon.name: "edit-paste"
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        flat: true
                        onClicked: {
                            taskNoteField.text = modelData;
                        }
                    }
                }
            }
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Save Note")
                icon.name: "document-save"
                enabled: taskNoteField.text.trim().length > 0
                onTriggered: root.saveTaskNote(taskNoteField.text)
            },
            Kirigami.Action {
                text: i18n("Skip")
                icon.name: "dialog-cancel"
                onTriggered: taskNoteDialog.close()
            }
        ]

        onOpened: {
            taskNoteField.text = "";
            recentRepeater.model = DataManager.recentTaskNotes(5);
            taskNoteField.forceActiveFocus();
        }
    }

    // The resume prompt waits while the window is hidden (e.g. in the tray) and appears once the
    // window is shown again, if the timer is still paused because of the lock.
    property bool screenLockPromptPending: false

    function showPendingScreenLockPrompt(): void {
        if (!screenLockPromptPending || !root.visible || !root.active) {
            return;
        }
        screenLockPromptPending = false;
        if (TimerEngine.paused && ScreenLockWatcher.pausedByScreenLock) {
            screenLockResumeDialog.open();
        }
    }

    onActiveChanged: showPendingScreenLockPrompt()
    onVisibleChanged: showPendingScreenLockPrompt()

    Connections {
        target: ScreenLockWatcher
        function onScreenUnlockedAfterPause() {
            if (!TimerEngine.paused) {
                return;
            }
            root.screenLockPromptPending = true;
            root.showPendingScreenLockPrompt();
        }
        function onPausedByScreenLockChanged() {
            if (!ScreenLockWatcher.pausedByScreenLock) {
                root.screenLockPromptPending = false;
            }
        }
    }

    Kirigami.PromptDialog {
        id: screenLockResumeDialog
        preferredWidth: Kirigami.Units.gridUnit * 24
        title: i18n("Screen Unlocked")
        subtitle: i18n("The timer was paused while your screen was locked. Would you like to resume?")
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Resume")
                icon.name: "media-playback-start"
                onTriggered: {
                    ScreenLockWatcher.clearLockPauseNotice();
                    TimerEngine.resume();
                    screenLockResumeDialog.close();
                }
            },
            Kirigami.Action {
                text: i18n("Keep Paused")
                icon.name: "media-playback-pause"
                onTriggered: {
                    ScreenLockWatcher.clearLockPauseNotice();
                    screenLockResumeDialog.close();
                }
            }
        ]
    }
}
