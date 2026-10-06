// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

/// Circular progress indicator; children are laid out inside the ring.
Item {
    id: root

    /// 0..1, drawn clockwise from the top.
    property real progress: 0
    property color color: Kirigami.Theme.highlightColor
    property color trackColor: Qt.alpha(Kirigami.Theme.textColor, 0.1)
    property real lineWidth: Kirigami.Units.gridUnit * 0.8

    // Drawn value: eases forward in small steps; jumps (no animation) when the progress resets.
    property real shown: progress
    onProgressChanged: {
        shownAnim.enabled = progress > shown;
        shown = progress;
    }
    Behavior on shown {
        id: shownAnim
        NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic }
    }

    default property alias content: contentItem.data

    readonly property real radius: (Math.min(width, height) - lineWidth) / 2

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.trackColor
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.radius
                radiusY: root.radius
                startAngle: -90
                sweepAngle: 360
            }
        }

        ShapePath {
            // Fully transparent at 0 so the round cap does not draw a dot.
            strokeColor: root.shown > 0 ? root.color : "transparent"
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.radius
                radiusY: root.radius
                startAngle: -90
                sweepAngle: 360 * Math.min(1, Math.max(0, root.shown))
            }
        }
    }

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
