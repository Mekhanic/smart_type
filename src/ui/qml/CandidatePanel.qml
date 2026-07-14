import QtQuick
import QtQuick.Window
import QtQuick.Effects

Window {
    id: panel
    required property var candidateModel
    property int selectedIndex: 0
    property real desiredX: 0
    property real desiredY: 0
    property int blurRefreshTicks: 0
    property bool screenshotMode: false
    signal candidateChosen(int index)

    flags: (screenshotMode ? Qt.Tool : Qt.Popup) | Qt.FramelessWindowHint |
           Qt.NoDropShadowWindowHint | Qt.WindowDoesNotAcceptFocus
    color: "transparent"
    
    // We add 100px padding to the left and right, and 20px padding to the top and bottom.
    // This allows the panel to move smoothly inside the window without compositor window movements.
    width: row.implicitWidth + 30 + 200
    height: 70 + 40
    visible: true
    
    // Position the window itself instantly to avoid compositor stutter.
    x: desiredX - 100
    y: desiredY - 20

    // Smoothly animated target coordinates
    property real smoothX: desiredX
    property real smoothY: desiredY
    Behavior on smoothX { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on smoothY { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    Item {
        id: visualContent
        width: row.implicitWidth + 30
        height: 70
        
        // Translate visualContent relative to the window coordinates
        x: smoothX - panel.x
        y: smoothY - panel.y

        // Selection shadow source
        Rectangle {
            id: pillShadowSource
            x: selectionPill.x
            y: selectionPill.y + 3
            width: selectionPill.width
            height: selectionPill.height
            radius: selectionPill.radius
            color: "#90000000"
            visible: false
        }

        MultiEffect {
            source: pillShadowSource
            anchors.fill: pillShadowSource
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: "#b0000000"
            shadowBlur: 0.7
            shadowVerticalOffset: 4
            opacity: selectionPill.opacity
        }

        // Shared animated selection pill (liquid glass styled)
        Rectangle {
            id: selectionPill
            visible: selectedIndex >= 0 && selectedIndex < repeater.count
            property var targetItem: visible ? repeater.itemAt(selectedIndex) : null

            x: targetItem ? row.x + targetItem.x + 4 : 0
            y: targetItem ? row.y + targetItem.y + 4 : 0
            width: targetItem ? targetItem.width - 8 : 0
            height: targetItem ? targetItem.height - 8 : 0
            radius: height / 2

            color: "#3dffffff"
            border.width: 1.0
            border.color: "#a0ffffff"
            clip: true

            // Fluid spring animation for the selection indicator
            Behavior on x { SpringAnimation { spring: 4.2; damping: 0.48; mass: 0.7 } }
            Behavior on y { SpringAnimation { spring: 4.2; damping: 0.48; mass: 0.7 } }
            Behavior on width { SpringAnimation { spring: 4.2; damping: 0.48; mass: 0.7 } }
            Behavior on height { SpringAnimation { spring: 4.2; damping: 0.48; mass: 0.7 } }

            // Liquid glass color gradient matching the theme
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#552dd7ff" }
                    GradientStop { position: 0.58; color: "#556652ff" }
                    GradientStop { position: 1.0; color: "#44d65cff" }
                }
            }

            // Inner highlight border
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1.0
                radius: parent.radius
                color: "transparent"
                border.width: 0.5
                border.color: "#80ffffff"
            }

            // Reflection glint
            Rectangle {
                x: 12
                y: 1.5
                width: parent.width - 24
                height: Math.max(6, parent.height * 0.25)
                radius: height / 2
                gradient: Gradient {
                    GradientStop { position: 0; color: "#80ffffff" }
                    GradientStop { position: 1; color: "#00ffffff" }
                }
                opacity: 0.7
            }
        }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Repeater {
                id: repeater
                model: panel.candidateModel
                CandidateCapsule {
                    selected: index === panel.selectedIndex
                    onChosen: function(candidateIndex) { panel.candidateChosen(candidateIndex) }
                }
            }
        }
    }

    function updateBlurRegion() {
        const areas = []
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item) areas.push({
                x: visualContent.x + row.x + item.x + 3,
                y: visualContent.y + row.y + item.y + 3,
                width: item.width - 6,
                height: item.height - 6,
                radius: (item.height - 6) / 2
            })
        }
        blurController.updateBlur(panel, areas)
    }

    function scheduleBlurRefresh() { blurRefreshTicks = 12 }

    onWidthChanged: scheduleBlurRefresh()
    onHeightChanged: scheduleBlurRefresh()
    onSelectedIndexChanged: scheduleBlurRefresh()
    Component.onCompleted: {
        opacity = 0
        scale = 0.9
        entrance.start()
        scheduleBlurRefresh()
    }

    ParallelAnimation {
        id: entrance
        NumberAnimation { target: panel; property: "opacity"; from: 0; to: 1; duration: 170; easing.type: Easing.OutCubic }
        NumberAnimation { target: panel; property: "scale"; from: 0.9; to: 1; duration: 330; easing.type: Easing.OutBack }
    }

    Timer {
        interval: 40
        running: panel.visible && panel.blurRefreshTicks > 0
        repeat: true
        onTriggered: {
            panel.updateBlurRegion()
            panel.blurRefreshTicks--
        }
    }
}
