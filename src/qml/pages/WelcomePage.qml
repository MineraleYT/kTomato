// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.mineraleyt.ktomato

Kirigami.Page {
    id: welcomePage

    title: i18n("Welcome")

    padding: 0

    readonly property real contentMaxWidth: Kirigami.Units.gridUnit * 36

    // A slide of the tour: centered when it fits, scrollable when the window is too short.
    // Content fades and slides in a little when the slide becomes the current one.
    component Slide: Flickable {
        id: slide
        default property alias content: slideColumn.data

        readonly property bool current: QQC2.SwipeView.isCurrentItem
        // Decorative animations run only while this slide is on screen and motion is enabled.
        readonly property bool animating: welcomePage.visible && current && Kirigami.Units.longDuration > 0
        property real reveal: current ? 1 : 0
        Behavior on reveal {
            NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
        }

        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: Math.max(height, slideColumn.implicitHeight + Kirigami.Units.gridUnit * 2)
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

        Item {
            width: slide.width
            height: slide.contentHeight

            ColumnLayout {
                id: slideColumn
                width: Math.min(slide.width - Kirigami.Units.gridUnit * 2, welcomePage.contentMaxWidth)
                x: (parent.width - width) / 2
                y: Math.max(Kirigami.Units.gridUnit, (parent.height - height) / 2) + (1 - slide.reveal) * Kirigami.Units.gridUnit
                opacity: 0.3 + 0.7 * slide.reveal
                spacing: Kirigami.Units.largeSpacing
            }
        }
    }

    // Thin circular progress arc (drawn from the top, clockwise). Cheap enough to animate per frame.
    component Arc: Shape {
        id: arc
        property real progress: 0
        property color color: Kirigami.Theme.highlightColor
        property color trackColor: Qt.alpha(Kirigami.Theme.textColor, 0.1)
        property real lineWidth: 2
        readonly property real radius: (Math.min(width, height) - lineWidth) / 2
        preferredRendererType: Shape.CurveRenderer
        // Never intercept clicks.
        containsMode: Shape.BoundingRectContains
        enabled: false

        ShapePath {
            strokeColor: arc.trackColor
            strokeWidth: arc.lineWidth
            fillColor: "transparent"
            PathAngleArc {
                centerX: arc.width / 2
                centerY: arc.height / 2
                radiusX: arc.radius
                radiusY: arc.radius
                startAngle: -90
                sweepAngle: 360
            }
        }
        ShapePath {
            // Fully transparent at 0 so the round cap does not draw a dot.
            strokeColor: arc.progress > 0 ? arc.color : "transparent"
            strokeWidth: arc.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: arc.width / 2
                centerY: arc.height / 2
                radiusX: arc.radius
                radiusY: arc.radius
                startAngle: -90
                sweepAngle: 360 * Math.min(1, Math.max(0, arc.progress))
            }
        }
    }

    // Circular accent badge holding an icon.
    component IconBadge: Item {
        id: badge
        property string source
        property color tint: Kirigami.Theme.highlightColor
        property int size: Kirigami.Units.iconSizes.large
        // The icon itself, so a slide can animate it (the bell rings, for instance).
        property alias iconItem: badgeIcon
        // 0..1 shows a thin progress arc around the badge; negative hides it.
        property real progress: -1
        // Swing angle of the icon (a bell rings), pivoting near the top of the glyph while the
        // icon stays centred in the circle (scale grows around the centre).
        property real swing: 0
        // Small optical correction for glyphs that sit off-centre in their box.
        property real glyphOffsetY: 0
        // Optional drawn micro-scene that replaces the icon; it fills the badge.
        property Component glyph: null
        implicitWidth: size
        implicitHeight: size

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.alpha(badge.tint, 0.15)
            Behavior on color { ColorAnimation { duration: 300 } }
        }
        Arc {
            anchors.fill: parent
            visible: badge.progress >= 0
            progress: badge.progress
            color: badge.tint
            Behavior on color { ColorAnimation { duration: 300 } }
        }
        Kirigami.Icon {
            id: badgeIcon
            anchors.centerIn: parent
            width: Math.round(badge.size * 0.58)
            height: width
            source: badge.source
            color: badge.tint
            visible: badge.glyph === null
            transform: [
                Rotation { origin.x: badgeIcon.width / 2; origin.y: badgeIcon.height * 0.14; angle: badge.swing },
                Translate { y: badge.glyphOffsetY }
            ]
            Behavior on color { ColorAnimation { duration: 300 } }
        }
        Loader {
            anchors.fill: parent
            active: badge.glyph !== null
            sourceComponent: badge.glyph
        }
    }

    // One rounded segment of a timeline bar. `reveal` (0..1) draws it in from the left,
    // `fill` (0..1) paints a brighter progress overlay; width and height give its full size.
    component TimelineSeg: Item {
        id: seg
        property color tint
        property real reveal: 1
        property real fill: 0
        Item {
            width: seg.width * seg.reveal
            height: seg.height
            clip: true
            Rectangle {
                width: seg.width
                height: seg.height
                radius: height / 2
                color: seg.tint
                opacity: 0.6
            }
            Item {
                width: seg.width * seg.fill
                height: seg.height
                clip: true
                visible: seg.fill > 0
                Rectangle {
                    width: seg.width
                    height: seg.height
                    radius: height / 2
                    color: seg.tint
                }
            }
        }
    }

    // Flat Breeze card surface.
    component Card: Rectangle {
        id: card
        default property alias content: cardLayout.data
        property alias layout: cardLayout
        Layout.fillWidth: true
        implicitHeight: cardLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.alternateBackgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing
        }
    }

    // Card with a badge, a title and a description.
    component FeatureCard: Card {
        id: feature
        property string icon
        property string title
        property string description
        property color tint: Kirigami.Theme.highlightColor
        property real progress: -1
        property Component glyph: null

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            IconBadge {
                glyph: feature.glyph
                source: feature.icon
                tint: feature.tint
                progress: feature.progress
                size: Kirigami.Units.iconSizes.large + Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing / 2

                Kirigami.Heading {
                    level: 3
                    text: feature.title
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: feature.description
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }

    // Hero block: large badge, title and subtitle.
    component Hero: ColumnLayout {
        id: hero
        property string icon
        property color tint: Kirigami.Theme.highlightColor
        property string title
        property string subtitle
        property int titleLevel: 2
        property alias badge: heroBadge

        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        IconBadge {
            id: heroBadge
            visible: hero.icon.length > 0
            source: hero.icon
            tint: hero.tint
            size: Kirigami.Units.iconSizes.huge + Kirigami.Units.gridUnit
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }
        Kirigami.Heading {
            text: hero.title
            level: hero.titleLevel
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: hero.subtitle
            color: Kirigami.Theme.disabledTextColor
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }
    }

    // Small star that fades in, rises and fades out around its parent (decorative).
    component Sparkle: Kirigami.Icon {
        id: sparkle
        property real delay: 0
        property bool running: false
        property real phase: 0

        source: "starred-symbolic"
        color: Kirigami.Theme.neutralTextColor
        width: Kirigami.Units.iconSizes.small
        height: width
        opacity: phase < 0.25 ? phase / 0.25 : (1 - phase) / 0.75
        scale: 0.4 + 0.6 * Math.min(1, phase * 3)
        transform: Translate { y: -sparkle.phase * Kirigami.Units.gridUnit * 1.4 }

        SequentialAnimation {
            running: sparkle.running
            onRunningChanged: if (!running) sparkle.phase = 0
            PauseAnimation { duration: sparkle.delay }
            SequentialAnimation {
                loops: Animation.Infinite
                NumberAnimation { target: sparkle; property: "phase"; from: 0; to: 1; duration: 2000; easing.type: Easing.OutSine }
                PauseAnimation { duration: 700 }
            }
        }
    }

    // Colour blend used by the hero drawings.
    function mix(a, b, t) {
        return Qt.tint(a, Qt.alpha(b, Math.max(0, Math.min(1, t))));
    }

    // The kTomato tomato drawn in the app icon's 128x128 geometry, with growth stages:
    // sprout (stem + two leaves), swelling fruit, ripening (green to red), calyx and gloss.
    component TomatoArt: Item {
        id: art
        // Growth stages, 0..1. The defaults are the finished, ripe tomato.
        property real sprout: 1
        property real leaf: 1
        property real fruit: 1
        property real ripe: 1
        property real calyx: 1
        property real gloss: 1
        // -1..1, gentle sway of the leaves.
        property real sway: 0

        readonly property real crownY: 72 - 36 * fruit
        readonly property color leafColor: Kirigami.Theme.positiveTextColor
        readonly property color darkGreen: Qt.darker(Kirigami.Theme.positiveTextColor, 1.35)

        width: 128
        height: 128

        // Stem, from the ground up to the crown.
        Rectangle {
            x: 62
            width: 4
            radius: 2
            y: 116 - (116 - (art.crownY - 22)) * art.sprout
            height: 116 - y
            color: art.darkGreen
        }

        // Fruit.
        Item {
            id: bodyLayer
            anchors.fill: parent
            visible: art.fruit > 0.01
            transform: Scale { origin.x: 64; origin.y: 76; xScale: art.fruit; yScale: art.fruit }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                containsMode: Shape.BoundingRectContains
                enabled: false
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: RadialGradient {
                        centerX: 54; centerY: 63; centerRadius: 67
                        focalX: 54; focalY: 63
                        GradientStop { position: 0; color: welcomePage.mix(Qt.lighter(Kirigami.Theme.positiveTextColor, 1.3), Qt.lighter(Kirigami.Theme.negativeTextColor, 1.4), art.ripe) }
                        GradientStop { position: 1; color: welcomePage.mix(Kirigami.Theme.positiveTextColor, Qt.darker(Kirigami.Theme.negativeTextColor, 1.1), art.ripe) }
                    }
                    PathAngleArc { centerX: 64; centerY: 76; radiusX: 48; radiusY: 44; startAngle: 0; sweepAngle: 360 }
                }
            }
            // Small glossy highlight.
            Rectangle {
                x: 32; y: 50
                width: 20; height: 9
                radius: 4.5
                rotation: -38
                color: "white"
                opacity: 0.4 * art.gloss
            }
        }

        // Sprout leaves, which give way to the calyx as the fruit grows.
        Rectangle {
            x: 64; y: art.crownY - 6
            width: 26; height: 12
            topRightRadius: 12; bottomLeftRadius: 12; topLeftRadius: 2; bottomRightRadius: 2
            color: art.leafColor
            transformOrigin: Item.Left
            scale: art.leaf
            rotation: -22 - 68 * (1 - art.leaf) + art.sway * 5
            opacity: art.leaf * (1 - art.calyx)
            visible: opacity > 0.01
        }
        Rectangle {
            x: 64 - width; y: art.crownY - 6
            width: 26; height: 12
            topLeftRadius: 12; bottomRightRadius: 12; topRightRadius: 2; bottomLeftRadius: 2
            color: art.leafColor
            transformOrigin: Item.Right
            scale: art.leaf
            rotation: 22 + 68 * (1 - art.leaf) + art.sway * 5
            opacity: art.leaf * (1 - art.calyx)
            visible: opacity > 0.01
        }

        // Calyx of the icon: spiky leaves and the dark base.
        Item {
            id: calyxLayer
            anchors.fill: parent
            opacity: art.calyx
            visible: art.calyx > 0.01
            transform: [
                Scale { origin.x: 64; origin.y: 36; xScale: art.calyx; yScale: art.calyx },
                Rotation { origin.x: 64; origin.y: 36; angle: art.sway * 2.5 },
                Translate { y: art.crownY - 36 }
            ]
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                containsMode: Shape.BoundingRectContains
                enabled: false
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: art.leafColor
                    PathSvg { path: "M64 36 L52 22 L60 32 L44 28 L58 38 Z M64 36 L76 22 L68 32 L84 28 L70 38 Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: art.darkGreen
                    PathAngleArc { centerX: 64; centerY: 36; radiusX: 9; radiusY: 5; startAngle: 0; sweepAngle: 360 }
                }
            }
        }

        // Top of the stem, in front of the calyx like in the icon.
        Rectangle {
            x: 62; y: art.crownY - 22
            width: 4; height: 14
            radius: 2
            color: art.darkGreen
            opacity: art.sprout
        }
    }

    // The kTomato tray icon, imitating TrayController::trayIcon(): a crown, a dark dial,
    // a clockwise progress ring in the phase colour and the remaining minutes.
    component TrayDial: Item {
        id: dial
        property real progress: 0
        property bool isBreak: false
        // Total minutes of the running phase; the number counts down with the ring.
        readonly property int totalMinutes: isBreak ? 5 : 25
        readonly property int minutes: Math.max(1, Math.ceil(totalMinutes * (1 - progress) - 0.0001))
        property color primary: isBreak ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
        Behavior on primary { ColorAnimation { duration: 250 } }

        readonly property real crownW: Math.max(3, Math.round(width * 0.22))
        readonly property real crownH: Math.max(1.5, Math.round(width * 0.08))
        readonly property real stemW: Math.max(1.5, Math.round(width * 0.09))
        readonly property real stemH: Math.max(1, Math.round(width * 0.05))
        readonly property real dialTop: 0.5 + crownH + stemH
        readonly property real dialD: width - dialTop - 1
        readonly property real penWidth: Math.max(1.8, Math.round(width * 0.11))

        implicitWidth: Kirigami.Units.iconSizes.large
        implicitHeight: implicitWidth
        height: width

        Rectangle {
            x: (dial.width - dial.crownW) / 2; y: 0.5
            width: dial.crownW; height: dial.crownH
            radius: 1
            color: dial.primary
        }
        Rectangle {
            x: (dial.width - dial.stemW) / 2; y: 0.5 + dial.crownH
            width: dial.stemW; height: dial.stemH
            color: dial.primary
        }
        Rectangle {
            id: dialBody
            x: (dial.width - dial.dialD) / 2; y: dial.dialTop + 0.5
            width: dial.dialD; height: width
            radius: width / 2
            color: Qt.rgba(20 / 255, 23 / 255, 28 / 255, 240 / 255)
        }
        Arc {
            x: dialBody.x; y: dialBody.y
            width: dialBody.width; height: dialBody.height
            lineWidth: dial.penWidth
            progress: dial.progress
            color: dial.primary
            trackColor: Qt.alpha(dial.primary, 55 / 255)
        }
        Text {
            anchors.centerIn: dialBody
            anchors.verticalCenterOffset: 0
            text: dial.minutes
            color: "white"
            font.bold: true
            font.pixelSize: Math.round(dialBody.width * (text.length >= 2 ? 0.44 : 0.52))
        }
    }

    // Check boxes bound to a setting lose their binding when clicked; binding again afterwards
    // shows the real value, e.g. when the change was refused.
    component SettingCheckBox: QQC2.CheckBox {
        id: box
        required property string setting
        checked: AppSettings[setting]
        onToggled: {
            AppSettings[setting] = checked;
            checked = Qt.binding(() => AppSettings[box.setting]);
        }
    }

    // Work minutes of the selected timer, used to estimate the daily focus time.
    readonly property int workMinutes: PresetModel.currentWorkMinutes > 0 ? PresetModel.currentWorkMinutes : 25

    property string autostartError: ""

    Connections {
        target: AppSettings
        function onAutostartFailed(message) {
            welcomePage.autostartError = message;
        }
        function onStartAtLoginChanged() {
            welcomePage.autostartError = "";
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- Multi-Step SwipeView ---
        QQC2.SwipeView {
            id: swipeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0
            clip: true

            // === Slide 1: Welcome & Overview ===
            Slide {
                id: slide1

                // Hero story (about 13.6 s, looping): a sprout grows and a tomato ripens, then it
                // flies into a mini Plasma panel and becomes the kTomato tray icon, which counts
                // down (time-lapse) and flips from work (red) to break (green). The stage has a
                // fixed size, so nothing around it moves.
                Item {
                    id: heroStage

                    readonly property real artSize: Kirigami.Units.iconSizes.enormous
                    readonly property real panelH: Kirigami.Units.gridUnit * 2.8
                    readonly property real panelGap: Kirigami.Units.gridUnit * 0.6
                    readonly property real dialSize: Math.round(panelH * 0.8)
                    readonly property real homeX: width / 2
                    readonly property real homeY: artSize / 2
                    readonly property real slotX: heroPanel.x + heroPanel.width - Kirigami.Units.gridUnit * 0.9
                        - Kirigami.Units.gridUnit * 2.6 - Kirigami.Units.gridUnit * 0.9 - dialSize / 2
                    readonly property real slotY: heroPanel.y + panelH / 2
                    readonly property real baseScale: artSize / 128
                    readonly property real endScale: dialSize / 110

                    // Animated state; the defaults are the calm resting picture.
                    property real flyX: 0
                    property real flyY: 0
                    property real pop: 0
                    property real artOpacity: 1
                    property real groundOpacity: 1
                    property real panelIn: 1
                    property real dialOpacity: 1
                    property real dialScale: 1
                    property real dialProgress: 0
                    property bool dialBreak: false

                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 16
                    Layout.preferredHeight: artSize + panelGap + panelH

                    function rest() {
                        heroTomatoIcon.sprout = 1; heroTomatoIcon.leaf = 1; heroTomatoIcon.fruit = 1;
                        heroTomatoIcon.ripe = 1; heroTomatoIcon.calyx = 1; heroTomatoIcon.gloss = 1;
                        heroTomatoIcon.sway = 0;
                        flyX = 0; flyY = 0; pop = 0; artOpacity = 1; groundOpacity = 1; panelIn = 1;
                        dialOpacity = 1; dialScale = 1; dialProgress = 0; dialBreak = false;
                    }
                    function startOver() {
                        heroTomatoIcon.sprout = 0; heroTomatoIcon.leaf = 0; heroTomatoIcon.fruit = 0;
                        heroTomatoIcon.ripe = 0; heroTomatoIcon.calyx = 0; heroTomatoIcon.gloss = 0;
                        flyX = 0; flyY = 0; pop = 0; artOpacity = 1; groundOpacity = 1; panelIn = 0;
                        dialOpacity = 0; dialScale = 0.75; dialProgress = 0; dialBreak = false;
                    }

                    // Soil line under the sprout.
                    Rectangle {
                        x: heroStage.homeX - width / 2
                        y: heroStage.homeY + 58 * heroStage.baseScale
                        width: Kirigami.Units.gridUnit * 3
                        height: 3
                        radius: 1.5
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.18)
                        opacity: heroStage.groundOpacity
                    }

                    // Mini Plasma panel: placeholder dots, a tray slot and a blank clock.
                    Rectangle {
                        id: heroPanel
                        y: heroStage.artSize + heroStage.panelGap
                        width: parent.width
                        height: heroStage.panelH
                        radius: Kirigami.Units.cornerRadius
                        color: Kirigami.Theme.alternateBackgroundColor
                        border.width: 1
                        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
                        opacity: heroStage.panelIn
                        transform: Translate { y: (1 - heroStage.panelIn) * Kirigami.Units.smallSpacing * 2 }

                        Row {
                            x: Kirigami.Units.gridUnit * 0.9
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Kirigami.Units.gridUnit * 0.6
                            Repeater {
                                model: 4
                                Rectangle {
                                    required property int index
                                    width: Kirigami.Units.gridUnit * 0.7
                                    height: width
                                    radius: width / 2
                                    color: Qt.alpha(Kirigami.Theme.textColor, index === 0 ? 0.35 : 0.2)
                                }
                            }
                        }
                        // Other tray icons, dim.
                        Row {
                            x: heroStage.slotX - heroStage.dialSize / 2 - width - Kirigami.Units.gridUnit * 0.6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Kirigami.Units.gridUnit * 0.6
                            Repeater {
                                model: 2
                                Rectangle {
                                    width: Kirigami.Units.gridUnit * 0.9
                                    height: width
                                    radius: width / 2
                                    color: Qt.alpha(Kirigami.Theme.textColor, 0.2)
                                }
                            }
                        }
                        // Clock-like blank.
                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: Kirigami.Units.gridUnit * 0.9
                            anchors.verticalCenter: parent.verticalCenter
                            width: Kirigami.Units.gridUnit * 2.6
                            height: Kirigami.Units.gridUnit * 0.8
                            radius: height / 2
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.22)
                        }
                    }

                    TrayDial {
                        id: heroDial
                        width: heroStage.dialSize
                        x: heroStage.slotX - width / 2
                        y: heroStage.slotY - height / 2
                        progress: heroStage.dialProgress
                        isBreak: heroStage.dialBreak
                        opacity: heroStage.dialOpacity * heroStage.panelIn
                        scale: heroStage.dialScale
                    }

                    TomatoArt {
                        id: heroTomatoIcon
                        x: heroStage.homeX + (heroStage.slotX - heroStage.homeX) * heroStage.flyX - width / 2
                        y: heroStage.homeY + (heroStage.slotY - heroStage.homeY) * heroStage.flyY - height / 2
                        scale: heroStage.baseScale * (1 + (heroStage.endScale / heroStage.baseScale - 1) * heroStage.flyY)
                            * (1 + 0.05 * heroStage.pop)
                        opacity: heroStage.artOpacity
                    }

                    // Leaves sway gently while the slide is on screen.
                    SequentialAnimation {
                        loops: Animation.Infinite
                        running: slide1.animating
                        onRunningChanged: if (!running) heroTomatoIcon.sway = 0
                        NumberAnimation { target: heroTomatoIcon; property: "sway"; to: 1; duration: 1500; easing.type: Easing.InOutSine }
                        NumberAnimation { target: heroTomatoIcon; property: "sway"; to: -1; duration: 1500; easing.type: Easing.InOutSine }
                    }

                    SequentialAnimation {
                        loops: Animation.Infinite
                        running: slide1.animating
                        onRunningChanged: if (!running) heroStage.rest()

                        ScriptAction { script: heroStage.startOver() }
                        PauseAnimation { duration: 300 }

                        // 0.3 - 6.3 s: sprout, swelling fruit, ripening.
                        ParallelAnimation {
                            NumberAnimation { target: heroTomatoIcon; property: "sprout"; to: 1; duration: 1300; easing.type: Easing.OutCubic }
                            SequentialAnimation {
                                PauseAnimation { duration: 600 }
                                NumberAnimation { target: heroTomatoIcon; property: "leaf"; to: 1; duration: 1000; easing.type: Easing.OutBack }
                            }
                            SequentialAnimation {
                                PauseAnimation { duration: 1700 }
                                NumberAnimation { target: heroTomatoIcon; property: "fruit"; to: 1; duration: 2800; easing.type: Easing.InOutSine }
                            }
                            SequentialAnimation {
                                PauseAnimation { duration: 2600 }
                                NumberAnimation { target: heroTomatoIcon; property: "calyx"; to: 1; duration: 1200; easing.type: Easing.OutCubic }
                            }
                            SequentialAnimation {
                                PauseAnimation { duration: 3000 }
                                NumberAnimation { target: heroTomatoIcon; property: "ripe"; to: 1; duration: 3000; easing.type: Easing.InOutSine }
                            }
                            SequentialAnimation {
                                PauseAnimation { duration: 4800 }
                                NumberAnimation { target: heroTomatoIcon; property: "gloss"; to: 1; duration: 800 }
                            }
                        }
                        // 6.3 - 7.1 s: a little pop, then the panel slides in.
                        NumberAnimation { target: heroStage; property: "pop"; to: 1; duration: 160; easing.type: Easing.OutCubic }
                        NumberAnimation { target: heroStage; property: "pop"; to: 0; duration: 220; easing.type: Easing.OutBack }
                        NumberAnimation { target: heroStage; property: "panelIn"; to: 1; duration: 450; easing.type: Easing.OutCubic }
                        // 7.1 - 8.2 s: the tomato shrinks into the tray slot and becomes the tray icon.
                        ParallelAnimation {
                            NumberAnimation { target: heroStage; property: "flyX"; to: 1; duration: 950; easing.type: Easing.InOutSine }
                            NumberAnimation { target: heroStage; property: "flyY"; to: 1; duration: 950; easing.type: Easing.InOutCubic }
                            NumberAnimation { target: heroStage; property: "groundOpacity"; to: 0; duration: 300 }
                            SequentialAnimation {
                                PauseAnimation { duration: 650 }
                                NumberAnimation { target: heroStage; property: "artOpacity"; to: 0; duration: 300 }
                            }
                            SequentialAnimation {
                                PauseAnimation { duration: 650 }
                                ParallelAnimation {
                                    NumberAnimation { target: heroStage; property: "dialOpacity"; to: 1; duration: 300 }
                                    NumberAnimation { target: heroStage; property: "dialScale"; to: 1; duration: 350; easing.type: Easing.OutBack }
                                }
                            }
                        }
                        PauseAnimation { duration: 450 }
                        // Countdown: 25, 24, 23 ...
                        NumberAnimation { target: heroStage; property: "dialProgress"; to: 0.04; duration: 300; easing.type: Easing.OutCubic }
                        PauseAnimation { duration: 350 }
                        NumberAnimation { target: heroStage; property: "dialProgress"; to: 0.08; duration: 300; easing.type: Easing.OutCubic }
                        PauseAnimation { duration: 350 }
                        // ... time-lapse to the end of the phase.
                        NumberAnimation { target: heroStage; property: "dialProgress"; to: 1; duration: 1700; easing.type: Easing.InOutQuad }
                        // Phase flip: green break, a small bounce, the number shows 5.
                        ScriptAction { script: { heroStage.dialBreak = true; heroStage.dialProgress = 0; } }
                        ParallelAnimation {
                            SequentialAnimation {
                                NumberAnimation { target: heroStage; property: "dialScale"; to: 1.22; duration: 140; easing.type: Easing.OutCubic }
                                NumberAnimation { target: heroStage; property: "dialScale"; to: 1.0; duration: 320; easing.type: Easing.OutBack }
                                PauseAnimation { duration: 1040 }
                            }
                            NumberAnimation { target: heroStage; property: "dialProgress"; to: 0.15; duration: 1500 }
                        }
                        // Fade out and start again.
                        ParallelAnimation {
                            NumberAnimation { target: heroStage; property: "panelIn"; to: 0; duration: 350 }
                        }
                        PauseAnimation { duration: 250 }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        text: i18n("Welcome to kTomato")
                        level: 1
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: i18n("An independent Pomodoro timer for KDE Plasma 6 and other Linux desktops.")
                        color: Kirigami.Theme.disabledTextColor
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "preferences-desktop-locale"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        color: Kirigami.Theme.disabledTextColor
                    }

                    QQC2.ComboBox {
                        id: welcomeLanguageBox
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                        textRole: "text"
                        valueRole: "value"
                        Accessible.name: i18n("Language")
                        model: [
                            { text: "English", value: "en" },
                            { text: "Español", value: "es" },
                            { text: "Français", value: "fr" },
                            { text: "Italiano", value: "it" },
                            { text: "Deutsch", value: "de" }
                        ]
                        Component.onCompleted: syncSelection()
                        Connections {
                            target: AppSettings
                            function onLanguageChanged() {
                                welcomeLanguageBox.syncSelection();
                            }
                        }
                        function syncSelection() {
                            const eff = AppSettings.effectiveLanguage;
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].value === eff) {
                                    currentIndex = i;
                                    return;
                                }
                            }
                            currentIndex = 0;
                        }
                        onActivated: {
                            AppSettings.language = currentValue;
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    FeatureCard {
                        icon: "chronometer"
                        tint: Kirigami.Theme.negativeTextColor
                        title: i18n("Focus")
                        description: i18n("Boost focus and avoid burnout using structured work intervals and timed breaks.")
                        // A stopwatch whose hand sweeps round.
                        glyph: Item {
                            id: stopwatch
                            property real handAngle: 0
                            readonly property real d: Math.round(width * 0.56)
                            readonly property color c: Kirigami.Theme.negativeTextColor
                            Rectangle {
                                x: (stopwatch.width - width) / 2
                                y: (stopwatch.height - stopwatch.d) / 2 - height + 1
                                width: Math.round(stopwatch.width * 0.16); height: 3
                                radius: 1
                                color: stopwatch.c
                            }
                            Rectangle {
                                id: swFace
                                x: (stopwatch.width - width) / 2
                                y: (stopwatch.height - height) / 2 + 2
                                width: stopwatch.d; height: width
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: stopwatch.c
                            }
                            Rectangle {
                                x: swFace.x + swFace.width / 2 - width / 2
                                y: swFace.y + swFace.height / 2 - height
                                width: 2; height: swFace.height * 0.36
                                radius: 1
                                color: stopwatch.c
                                transform: Rotation { origin.x: 1; origin.y: swFace.height * 0.36; angle: stopwatch.handAngle }
                            }
                            Rectangle {
                                x: swFace.x + swFace.width / 2 - 2
                                y: swFace.y + swFace.height / 2 - 2
                                width: 4; height: 4; radius: 2
                                color: stopwatch.c
                            }
                            SequentialAnimation {
                                loops: Animation.Infinite
                                running: slide1.animating
                                onRunningChanged: if (!running) stopwatch.handAngle = 0
                                NumberAnimation { target: stopwatch; property: "handAngle"; from: 0; to: 360; duration: 2600; easing.type: Easing.InOutCubic }
                                PauseAnimation { duration: 500 }
                            }
                        }
                    }
                    FeatureCard {
                        icon: "kde"
                        tint: Kirigami.Theme.highlightColor
                        title: i18n("Plasma")
                        description: i18n("Native Plasma 6 desktop integration with Breeze design and system tray controls.")
                        // A soft ripple around the logo.
                        glyph: Item {
                            id: plasmaGlyph
                            property real ripple: 0
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width; height: width
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: Kirigami.Theme.highlightColor
                                opacity: 0.5 * (1 - plasmaGlyph.ripple)
                                scale: 0.7 + 0.5 * plasmaGlyph.ripple
                            }
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: Math.round(parent.width * 0.58); height: width
                                source: "kde"
                                color: Kirigami.Theme.highlightColor
                            }
                            SequentialAnimation {
                                loops: Animation.Infinite
                                running: slide1.animating
                                onRunningChanged: if (!running) plasmaGlyph.ripple = 0
                                NumberAnimation { target: plasmaGlyph; property: "ripple"; from: 0; to: 1; duration: 1800; easing.type: Easing.OutSine }
                                PauseAnimation { duration: 900 }
                            }
                        }
                    }
                    FeatureCard {
                        icon: "security-high"
                        tint: Kirigami.Theme.positiveTextColor
                        title: i18n("Private")
                        description: i18n("100% private and offline: your timers and productivity stats stay on your device.")
                        // A padlock that opens, closes and shows a check.
                        glyph: Item {
                            id: padlock
                            property real open: 0
                            property real check: 0
                            readonly property color c: Kirigami.Theme.positiveTextColor
                            readonly property real bw: Math.round(width * 0.46)
                            readonly property real bh: Math.round(width * 0.34)
                            readonly property real bx: (width - bw) / 2
                            readonly property real by: Math.round(height * 0.5)
                            // Shackle: a rounded outline that lifts and swings open.
                            Rectangle {
                                id: shackle
                                x: padlock.bx + padlock.bw * 0.2
                                y: padlock.by - height + 2
                                width: padlock.bw * 0.6
                                height: Math.round(padlock.width * 0.3)
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: padlock.c
                                transform: [
                                    Rotation { origin.x: shackle.width; origin.y: shackle.height; angle: -34 * padlock.open },
                                    Translate { y: -padlock.width * 0.1 * padlock.open }
                                ]
                            }
                            Rectangle {
                                x: padlock.bx; y: padlock.by
                                width: padlock.bw; height: padlock.bh
                                radius: 3
                                color: padlock.c
                                // Keyhole.
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 4; height: 4; radius: 2
                                    color: Kirigami.Theme.backgroundColor
                                    opacity: 1 - padlock.check
                                }
                                // Check mark.
                                Shape {
                                    anchors.centerIn: parent
                                    width: 10; height: 8
                                    opacity: padlock.check
                                    scale: 0.6 + 0.4 * padlock.check
                                    preferredRendererType: Shape.CurveRenderer
                                    ShapePath {
                                        strokeColor: Kirigami.Theme.backgroundColor
                                        strokeWidth: 2
                                        fillColor: "transparent"
                                        capStyle: ShapePath.RoundCap
                                        joinStyle: ShapePath.RoundJoin
                                        startX: 1; startY: 4
                                        PathLine { x: 4; y: 7 }
                                        PathLine { x: 9; y: 1 }
                                    }
                                }
                            }
                            SequentialAnimation {
                                loops: Animation.Infinite
                                running: slide1.animating
                                onRunningChanged: if (!running) { padlock.open = 0; padlock.check = 0; }
                                PauseAnimation { duration: 1200 }
                                NumberAnimation { target: padlock; property: "open"; to: 1; duration: 380; easing.type: Easing.OutBack }
                                PauseAnimation { duration: 700 }
                                NumberAnimation { target: padlock; property: "open"; to: 0; duration: 260; easing.type: Easing.InQuad }
                                NumberAnimation { target: padlock; property: "check"; to: 1; duration: 260; easing.type: Easing.OutBack }
                                PauseAnimation { duration: 900 }
                                NumberAnimation { target: padlock; property: "check"; to: 0; duration: 260 }
                            }
                        }
                    }
                }
            }

            // === Slide 2: Timer Presets ===
            Slide {
                id: slide2

                // Index of the card showing the "selected preset" highlight (-1: none).
                property int activeIndex: -1

                Hero {
                    icon: "view-list-details"
                    title: i18n("Customizable Timer Presets")
                    subtitle: i18n("Switch between ready-to-use presets or design your own focus routines:")
                }

                GridLayout {
                    columns: slide2.width >= Kirigami.Units.gridUnit * 28 ? 2 : 1
                    Layout.fillWidth: true
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.largeSpacing

                    Repeater {
                        model: [
                            { name: i18n("Pomodoro"), category: i18n("Work"), work: 25, brk: 5, icon: "chronometer" },
                            { name: i18n("Deep Work"), category: i18n("Focus"), work: 50, brk: 10, icon: "flash-symbolic" },
                            { name: i18n("Study"), category: i18n("Study"), work: 45, brk: 15, icon: "view-readermode-symbolic" },
                            { name: i18n("Quick Sprint"), category: i18n("Tasks"), work: 15, brk: 3, icon: "speedometer-symbolic" }
                        ]

                        delegate: Card {
                            id: presetCard
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop

                            // Entrance (staggered) and highlight state; only transforms and opacity change.
                            property real enter: 1
                            property real hl: slide2.activeIndex === index ? 1 : 0
                            // Timeline bar: draw-in progress of the work/break segments, playhead position
                            // (0..1 along this preset's own bar), playhead opacity and end-of-bar tick.
                            property real drawWork: 1
                            property real drawBreak: 1
                            property real head: 0
                            property real headOpacity: 0
                            property real tick: 1
                            Behavior on hl { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                            opacity: enter
                            scale: 1 + 0.02 * hl
                            transform: Translate { y: (1 - presetCard.enter) * Kirigami.Units.gridUnit }

                            SequentialAnimation {
                                running: slide2.animating
                                onRunningChanged: if (!running) {
                                    presetCard.enter = 1;
                                    presetCard.drawWork = 1;
                                    presetCard.drawBreak = 1;
                                }
                                ScriptAction { script: { presetCard.enter = 0; presetCard.drawWork = 0; presetCard.drawBreak = 0; } }
                                PauseAnimation { duration: 120 + presetCard.index * 110 }
                                NumberAnimation { target: presetCard; property: "enter"; to: 1; duration: 360; easing.type: Easing.OutCubic }
                                // The bar draws in left to right: work first, then the break.
                                NumberAnimation { target: presetCard; property: "drawWork"; to: 1; duration: 380; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: presetCard; property: "drawBreak"; to: 1; duration: 220; easing.type: Easing.OutQuad }
                            }

                            // Playhead run while this card is highlighted; lasts in proportion to the
                            // preset's real length (1.1 s for a 60 min total), then ticks and resets.
                            SequentialAnimation {
                                running: slide2.activeIndex === presetCard.index
                                onRunningChanged: if (!running) {
                                    presetCard.head = 0;
                                    presetCard.headOpacity = 0;
                                    presetCard.tick = 1;
                                }
                                PauseAnimation { duration: 60 }
                                NumberAnimation { target: presetCard; property: "headOpacity"; to: 1; duration: 60 }
                                NumberAnimation {
                                    target: presetCard
                                    property: "head"
                                    from: 0
                                    to: 1
                                    duration: Math.round(1100 * (presetCard.modelData.work + presetCard.modelData.brk) / 60)
                                    easing.type: Easing.Linear
                                }
                                ParallelAnimation {
                                    NumberAnimation { target: presetCard; property: "headOpacity"; to: 0; duration: 220 }
                                    SequentialAnimation {
                                        NumberAnimation { target: presetCard; property: "tick"; to: 1.7; duration: 90; easing.type: Easing.OutQuad }
                                        NumberAnimation { target: presetCard; property: "tick"; to: 1; duration: 150; easing.type: Easing.OutBack }
                                    }
                                }
                                ScriptAction { script: presetCard.head = 0 }
                            }

                            color: Qt.tint(Kirigami.Theme.alternateBackgroundColor, Qt.alpha(Kirigami.Theme.highlightColor, 0.12 * hl))
                            border.color: Qt.tint(Qt.alpha(Kirigami.Theme.textColor, 0.15), Qt.alpha(Kirigami.Theme.highlightColor, hl))

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.largeSpacing

                                IconBadge {
                                    source: presetCard.modelData.icon
                                    tint: Kirigami.Theme.negativeTextColor
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing / 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        Kirigami.Heading {
                                            level: 3
                                            text: presetCard.modelData.name
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // Category chip
                                        Rectangle {
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitWidth: chipLabel.implicitWidth + Kirigami.Units.largeSpacing
                                            implicitHeight: chipLabel.implicitHeight + Kirigami.Units.smallSpacing
                                            radius: height / 2
                                            color: Qt.alpha(Kirigami.Theme.highlightColor, 0.15)
                                            QQC2.Label {
                                                id: chipLabel
                                                anchors.centerIn: parent
                                                text: presetCard.modelData.category
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.highlightColor
                                            }
                                        }
                                    }

                                    RowLayout {
                                        spacing: Kirigami.Units.smallSpacing
                                        Rectangle {
                                            Layout.preferredWidth: Kirigami.Units.smallSpacing * 2
                                            Layout.preferredHeight: Layout.preferredWidth
                                            radius: width / 2
                                            color: Kirigami.Theme.negativeTextColor
                                        }
                                        QQC2.Label {
                                            text: i18nc("%1m work, %2m break", "%1m / %2m", presetCard.modelData.work, presetCard.modelData.brk)
                                            color: Kirigami.Theme.disabledTextColor
                                        }
                                    }
                                }
                            }

                            // Work/break timeline, drawn to a shared scale (60 min fills the width).
                            Item {
                                id: timeline
                                readonly property real gap: 2
                                readonly property real perMin: width / 60
                                readonly property real total: presetCard.modelData.work + presetCard.modelData.brk
                                readonly property real workW: Math.max(1, presetCard.modelData.work * perMin - gap / 2)
                                readonly property real breakX: presetCard.modelData.work * perMin + gap / 2
                                readonly property real breakW: Math.max(1, presetCard.modelData.brk * perMin - gap / 2)
                                readonly property real headX: presetCard.head * total * perMin
                                readonly property real barH: Math.round(Kirigami.Units.smallSpacing * 1.5)
                                Layout.fillWidth: true
                                Layout.topMargin: Kirigami.Units.smallSpacing
                                Layout.preferredHeight: barH + Kirigami.Units.smallSpacing * 2

                                TimelineSeg {
                                    y: (timeline.height - timeline.barH) / 2
                                    width: timeline.workW
                                    height: timeline.barH
                                    tint: Kirigami.Theme.negativeTextColor
                                    reveal: presetCard.drawWork
                                    fill: Math.max(0, Math.min(1, timeline.headX / timeline.workW))
                                    transform: Scale { origin.y: timeline.barH / 2; yScale: presetCard.tick }
                                }
                                TimelineSeg {
                                    x: timeline.breakX
                                    y: (timeline.height - timeline.barH) / 2
                                    width: timeline.breakW
                                    height: timeline.barH
                                    tint: Kirigami.Theme.positiveTextColor
                                    reveal: presetCard.drawBreak
                                    fill: Math.max(0, Math.min(1, (timeline.headX - timeline.breakX) / timeline.breakW))
                                    transform: Scale { origin.y: timeline.barH / 2; yScale: presetCard.tick }
                                }

                                // Playhead: soft glow plus a thin line.
                                Item {
                                    x: timeline.headX
                                    anchors.verticalCenter: parent.verticalCenter
                                    opacity: presetCard.headOpacity
                                    visible: opacity > 0
                                    Rectangle {
                                        x: -width / 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Kirigami.Units.smallSpacing * 2
                                        height: timeline.height
                                        radius: width / 2
                                        color: Qt.alpha(Kirigami.Theme.textColor, 0.18)
                                    }
                                    Rectangle {
                                        x: -width / 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 2
                                        height: timeline.height - 2
                                        radius: 1
                                        color: Kirigami.Theme.textColor
                                    }
                                }
                            }
                        }
                    }
                }

                // Hops the highlight from card to card, as if switching presets.
                SequentialAnimation {
                    running: slide2.animating
                    onRunningChanged: if (!running) slide2.activeIndex = -1
                    PauseAnimation { duration: 1100 }
                    SequentialAnimation {
                        loops: Animation.Infinite
                        ScriptAction { script: slide2.activeIndex = 0 }
                        PauseAnimation { duration: 1600 }
                        ScriptAction { script: slide2.activeIndex = 1 }
                        PauseAnimation { duration: 1600 }
                        ScriptAction { script: slide2.activeIndex = 2 }
                        PauseAnimation { duration: 1600 }
                        ScriptAction { script: slide2.activeIndex = 3 }
                        PauseAnimation { duration: 1600 }
                        ScriptAction { script: slide2.activeIndex = -1 }
                        PauseAnimation { duration: 500 }
                    }
                }

                QQC2.Label {
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    text: i18n("You can edit, duplicate, and assign custom symbolic icons to any preset in the Timers tab.")
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            // === Slide 3: Plasma & Do Not Disturb Integration ===
            Slide {
                id: slide3

                // Animation state. One 10 s loop tells a small story: during the work phase a
                // notification arrives, the bell rings, Do Not Disturb silences it (green, bounce);
                // in the break phase notifications are allowed again. The chronometer card fills
                // its arc in the work colour, then in the break colour.
                property bool bellSilenced: false
                property real chronoProgress: 0.0
                property bool chronoIsBreak: false
                property real toastSlide: 1      // 1: off to the right, 0: settled
                property real toastOpacity: 0
                property real toastScale: 1

                Hero {
                    id: heroBell
                    icon: slide3.bellSilenced ? "notifications-disabled" : "notifications"
                    tint: slide3.bellSilenced ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.highlightColor
                    title: i18n("Distraction-Free Focus")
                    subtitle: i18n("During work phases, kTomato can silence desktop notifications and request that the screen stay awake.")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.mediumSpacing

                    FeatureCard {
                        icon: "notifications-disabled"
                        tint: Kirigami.Theme.positiveTextColor
                        title: i18n("Automatic Do Not Disturb")
                        description: i18n("Notifications are silenced during work intervals and safely restored during breaks.")
                    }
                    FeatureCard {
                        icon: "chronometer"
                        tint: slide3.chronoIsBreak ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor
                        progress: slide3.animating ? slide3.chronoProgress : -1
                        title: i18n("Dynamic Panel Chronometer")
                        description: i18n("A live tray icon shows your phase (Red for work, Green for breaks) with remaining minutes and circular progress.")
                    }
                }

                // Decorative toast next to the bell: app icon and skeleton lines, no text.
                Rectangle {
                    id: toast
                    parent: heroBell.badge
                    x: heroBell.badge.width + Kirigami.Units.smallSpacing + slide3.toastSlide * Kirigami.Units.gridUnit * 2
                    y: (heroBell.badge.height - height) / 2
                    width: Kirigami.Units.gridUnit * 5.5
                    height: Kirigami.Units.gridUnit * 2.4
                    radius: Kirigami.Units.cornerRadius
                    color: Kirigami.Theme.backgroundColor
                    border.width: 1
                    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)
                    opacity: slide3.toastOpacity
                    scale: slide3.toastScale
                    transformOrigin: Item.Right

                    Kirigami.Icon {
                        id: toastIcon
                        x: Kirigami.Units.smallSpacing
                        anchors.verticalCenter: parent.verticalCenter
                        width: Kirigami.Units.iconSizes.smallMedium
                        height: width
                        source: "io.github.mineraleyt.ktomato"
                    }
                    Column {
                        anchors.left: toastIcon.right
                        anchors.leftMargin: Kirigami.Units.smallSpacing
                        anchors.right: parent.right
                        anchors.rightMargin: Kirigami.Units.largeSpacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            width: parent.width * 0.85
                            height: 4
                            radius: 2
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.35)
                        }
                        Rectangle {
                            width: parent.width * 0.55
                            height: 4
                            radius: 2
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.18)
                        }
                    }
                }

                ParallelAnimation {
                    loops: Animation.Infinite
                    running: slide3.animating
                    onRunningChanged: {
                        if (!running) {
                            slide3.bellSilenced = false;
                            slide3.chronoIsBreak = false;
                            slide3.chronoProgress = 0;
                            slide3.toastOpacity = 0;
                            slide3.toastSlide = 1;
                            slide3.toastScale = 1;
                            heroBell.badge.swing = 0;
                            heroBell.badge.iconItem.scale = 1.0;
                        }
                    }

                    // Chronometer: work phase (red), then break phase (green).
                    SequentialAnimation {
                        ScriptAction { script: { slide3.chronoIsBreak = false; slide3.chronoProgress = 0; } }
                        NumberAnimation { target: slide3; property: "chronoProgress"; from: 0; to: 1; duration: 6000 }
                        ScriptAction { script: { slide3.chronoIsBreak = true; slide3.chronoProgress = 0; } }
                        NumberAnimation { target: slide3; property: "chronoProgress"; from: 0; to: 1; duration: 3600 }
                        PauseAnimation { duration: 400 }
                    }

                    // Notification story (10 s in total, same as the chronometer).
                    SequentialAnimation {
                        ScriptAction {
                            script: {
                                slide3.bellSilenced = false;
                                slide3.toastOpacity = 0;
                                slide3.toastSlide = 1;
                                slide3.toastScale = 1;
                            }
                        }
                        PauseAnimation { duration: 600 }

                        // A notification arrives and the bell rings.
                        ParallelAnimation {
                            NumberAnimation { target: slide3; property: "toastOpacity"; to: 1; duration: 260; easing.type: Easing.OutCubic }
                            NumberAnimation { target: slide3; property: "toastSlide"; to: 0; duration: 380; easing.type: Easing.OutCubic }
                            SequentialAnimation {
                                PauseAnimation { duration: 120 }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: 14; duration: 70; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: -14; duration: 70; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: 12; duration: 70; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: -12; duration: 70; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: 6; duration: 70; easing.type: Easing.InOutQuad }
                                NumberAnimation { target: heroBell.badge; property: "swing"; to: 0; duration: 70; easing.type: Easing.InOutQuad }
                            }
                        }
                        PauseAnimation { duration: 500 }

                        // Do Not Disturb: the bell is silenced with a bounce, the toast is blocked.
                        ScriptAction { script: slide3.bellSilenced = true }
                        ParallelAnimation {
                            SequentialAnimation {
                                NumberAnimation { target: heroBell.badge.iconItem; property: "scale"; to: 1.25; duration: 160; easing.type: Easing.OutBack }
                                NumberAnimation { target: heroBell.badge.iconItem; property: "scale"; to: 1.0; duration: 160; easing.type: Easing.InOutQuad }
                            }
                            NumberAnimation { target: slide3; property: "toastOpacity"; to: 0; duration: 340; easing.type: Easing.InQuad }
                            NumberAnimation { target: slide3; property: "toastScale"; to: 0.8; duration: 340; easing.type: Easing.InQuad }
                        }
                        PauseAnimation { duration: 4010 }

                        // Break: notifications are restored, a toast is allowed in and settles.
                        ScriptAction {
                            script: {
                                slide3.bellSilenced = false;
                                slide3.toastScale = 1;
                                slide3.toastSlide = 1;
                            }
                        }
                        ParallelAnimation {
                            NumberAnimation { target: slide3; property: "toastOpacity"; to: 1; duration: 260; easing.type: Easing.OutCubic }
                            NumberAnimation { target: slide3; property: "toastSlide"; to: 0; duration: 450; easing.type: Easing.OutBack }
                            SequentialAnimation {
                                NumberAnimation { target: heroBell.badge.iconItem; property: "scale"; to: 1.15; duration: 140; easing.type: Easing.OutCubic }
                                NumberAnimation { target: heroBell.badge.iconItem; property: "scale"; to: 1.0; duration: 200; easing.type: Easing.InOutQuad }
                            }
                        }
                        PauseAnimation { duration: 3150 }
                        NumberAnimation { target: slide3; property: "toastOpacity"; to: 0; duration: 300; easing.type: Easing.InQuad }
                        PauseAnimation { duration: 100 }
                    }
                }
            }

            // === Slide 4: Goals & Setup ===
            Slide {
                id: slide4

                Hero {
                    id: heroGoal
                    icon: "emblem-favorite"
                    tint: Kirigami.Theme.negativeTextColor
                    title: i18n("Personalize Your Goals")
                    subtitle: i18n("Set your initial daily target. You can adjust this anytime in Settings.")
                }

                Card {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Heading {
                            level: 3
                            text: i18n("Daily Pomodoro Goal:")
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        // 0 disables the daily goal.
                        QQC2.SpinBox {
                            id: goalSpinBox
                            from: 0
                            to: 50
                            value: AppSettings.dailyGoal
                            editable: true
                            Accessible.name: i18n("Daily Pomodoro Goal:")
                            // The text may be "Disabled": accept any text and read the number out of it.
                            validator: RegularExpressionValidator { regularExpression: /.*/ }
                            textFromValue: (value, locale) => value === 0 ? i18n("Disabled") : value.toString()
                            valueFromText: (text, locale) => {
                                const match = text.match(/\d+/);
                                if (match) {
                                    return parseInt(match[0]);
                                }
                                return text.trim() === i18n("Disabled") ? 0 : goalSpinBox.value;
                            }
                            onValueModified: AppSettings.dailyGoal = value
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: [0, 4, 6, 8, 10, 12]

                            delegate: QQC2.Button {
                                id: goalBtn
                                required property int modelData
                                width: Math.max(implicitWidth, (parent.width - Kirigami.Units.smallSpacing * 5) / 6)
                                text: modelData === 0 ? i18n("Disabled") : modelData.toLocaleString(Qt.locale(), 'f', 0)
                                // Not checkable: clicking the selected value must not uncheck it while
                                // the goal stays the same. The checked state follows the setting.
                                implicitHeight: Kirigami.Units.gridUnit * 2
                                checkable: false
                                checked: AppSettings.dailyGoal === modelData
                                Accessible.name: modelData === 0
                                    ? i18n("Disabled")
                                    : i18np("%1 pomodoro / day", "%1 pomodoros / day", modelData)
                                onClicked: AppSettings.dailyGoal = modelData

                                // Segmented pill look: the selected value is filled with the accent colour.
                                background: Rectangle {
                                    radius: Kirigami.Units.cornerRadius
                                    color: goalBtn.checked ? Kirigami.Theme.highlightColor
                                        : goalBtn.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.12)
                                        : Qt.alpha(Kirigami.Theme.textColor, 0.06)
                                    border.width: 1
                                    border.color: goalBtn.checked || goalBtn.visualFocus ? Kirigami.Theme.highlightColor
                                        : Qt.alpha(Kirigami.Theme.textColor, 0.15)
                                    Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
                                }
                                contentItem: QQC2.Label {
                                    text: goalBtn.text
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.weight: goalBtn.checked ? Font.DemiBold : Font.Normal
                                    color: goalBtn.checked ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                }
                            }
                        }
                    }

                    QQC2.Label {
                        id: goalEstimate
                        transformOrigin: Item.Left
                        SequentialAnimation {
                            id: goalBump
                            NumberAnimation { target: goalEstimate; property: "scale"; to: 1.08; duration: 90; easing.type: Easing.OutQuad }
                            NumberAnimation { target: goalEstimate; property: "scale"; to: 1.0; duration: 220; easing.type: Easing.OutBack }
                        }
                        text: AppSettings.dailyGoal > 0
                            ? i18np("%1 pomodoro / day (~%2h focus)", "%1 pomodoros / day (~%2h focus)",
                                    AppSettings.dailyGoal,
                                    Number(AppSettings.dailyGoal * welcomePage.workMinutes / 60).toLocaleString(Qt.locale(), 'f', 1))
                            : i18n("No daily goal set")
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Card {
                    SettingCheckBox {
                        Layout.fillWidth: true
                        setting: "protectWeekendStreak"
                        text: i18n("Protect weekend streak (don't break streak if inactive on weekends)")
                    }

                    SettingCheckBox {
                        Layout.fillWidth: true
                        visible: AppSettings.trayAvailable
                        setting: "closeToTray"
                        text: i18n("Keep running in system tray when window is closed")
                    }

                    SettingCheckBox {
                        Layout.fillWidth: true
                        visible: AppSettings.autostartSupported
                        setting: "startAtLogin"
                        text: i18n("Launch kTomato automatically on desktop login")
                    }

                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: welcomePage.autostartError.length > 0
                        type: Kirigami.MessageType.Error
                        text: welcomePage.autostartError
                    }
                }

                QQC2.Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    text: i18n("Get Started")
                    icon.name: "dialog-ok"
                    highlighted: true
                    onClicked: {
                        AppSettings.firstRunCompleted = true;
                        applicationWindow().navigate("timer");
                    }
                }

                // Small stars drifting up around the badge.
                Sparkle { parent: heroGoal.badge; x: heroGoal.badge.width * 0.00 - width / 2; y: heroGoal.badge.height * 0.20; delay: 0; running: slide4.animating }
                Sparkle { parent: heroGoal.badge; x: heroGoal.badge.width * 1.00 - width / 2; y: heroGoal.badge.height * 0.10; delay: 700; color: Kirigami.Theme.highlightColor; running: slide4.animating }
                Sparkle { parent: heroGoal.badge; x: heroGoal.badge.width * 0.88 - width / 2; y: heroGoal.badge.height * 0.80; delay: 1400; running: slide4.animating }
                Sparkle { parent: heroGoal.badge; x: heroGoal.badge.width * 0.12 - width / 2; y: heroGoal.badge.height * 0.85; delay: 2100; color: Kirigami.Theme.highlightColor; running: slide4.animating }

                // Heartbeat: a double pulse, then a rest. Only while this slide is on screen.
                SequentialAnimation {
                    loops: Animation.Infinite
                    running: slide4.animating
                    onRunningChanged: if (!running) heroGoal.badge.scale = 1.0
                    NumberAnimation { target: heroGoal.badge; property: "scale"; to: 1.12; duration: 120; easing.type: Easing.OutQuad }
                    NumberAnimation { target: heroGoal.badge; property: "scale"; to: 1.0; duration: 140; easing.type: Easing.InOutQuad }
                    NumberAnimation { target: heroGoal.badge; property: "scale"; to: 1.08; duration: 110; easing.type: Easing.OutQuad }
                    NumberAnimation { target: heroGoal.badge; property: "scale"; to: 1.0; duration: 200; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 1500 }
                }

                // Small bump of the estimate when the goal changes (pill or spin box).
                Connections {
                    target: AppSettings
                    function onDailyGoalChanged() {
                        if (slide4.animating) {
                            goalBump.restart();
                        }
                    }
                }
            }
        }

        // --- Bottom Controls ---
        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            implicitHeight: Math.max(skipButton.implicitHeight, nextButton.implicitHeight, indicator.implicitHeight)

            QQC2.Button {
                id: skipButton
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: i18n("Skip Tour")
                flat: true
                visible: swipeView.currentIndex < swipeView.count - 1
                onClicked: {
                    AppSettings.firstRunCompleted = true;
                    applicationWindow().navigate("timer");
                }
            }

            QQC2.PageIndicator {
                id: indicator
                anchors.centerIn: parent
                // Hide the dots when the window is too narrow to fit them between the buttons.
                visible: parent.width - 2 * Math.max(skipButton.implicitWidth, navButtons.implicitWidth)
                    - Kirigami.Units.largeSpacing > implicitWidth
                count: swipeView.count
                currentIndex: swipeView.currentIndex
                interactive: true
                onCurrentIndexChanged: swipeView.currentIndex = currentIndex

                delegate: Item {
                    id: dot
                    required property int index
                    readonly property bool active: index === indicator.currentIndex
                    implicitWidth: active ? Kirigami.Units.gridUnit * 1.5 : Kirigami.Units.smallSpacing * 2
                    implicitHeight: Kirigami.Units.smallSpacing * 2
                    Behavior on implicitWidth {
                        NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: dot.active ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, 0.3)
                        Behavior on color { ColorAnimation { duration: Kirigami.Units.longDuration } }
                    }
                }
            }

            RowLayout {
                id: navButtons
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    id: backButton
                    text: i18n("Back")
                    icon.name: "go-previous"
                    flat: true
                    visible: swipeView.currentIndex > 0
                    onClicked: swipeView.decrementCurrentIndex()
                }

                QQC2.Button {
                    id: nextButton
                    text: swipeView.currentIndex === swipeView.count - 1 ? i18n("Finish") : i18n("Next")
                    icon.name: swipeView.currentIndex === swipeView.count - 1 ? "dialog-ok" : "go-next"
                    highlighted: true
                    onClicked: {
                        if (swipeView.currentIndex === swipeView.count - 1) {
                            AppSettings.firstRunCompleted = true;
                            applicationWindow().navigate("timer");
                        } else {
                            swipeView.incrementCurrentIndex();
                        }
                    }
                }
            }
        }
    }
}
