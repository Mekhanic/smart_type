import QtQuick
import QtQuick.Window
import QtQuick.Effects
import org.kde.layershell 1.0 as LayerShell

Window {
    id: production
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint | Qt.WindowDoesNotAcceptFocus
    visible: (rendererBridge.panelVisible && rendererBridge.candidates.length > 0) || rendererBridge.flashes.length > 0 || isFlashing

    // Add 100px padding to the left and right, and 20px padding to the top and bottom.
    // This lets us animate the visible candidate panel smoothly inside the window
    // without invoking Wayland window movements.
    width: (isFlashing ? flashRow.implicitWidth : row.implicitWidth) + 24 + 200
    height: 58 + 40

    readonly property bool isFlashing: rendererBridge.flashId > 0
    property int blurRefreshTicks: 0
    property bool placeBelow: true

    // Target coordinates
    readonly property real windowTargetX: rendererBridge.targetX
    readonly property real windowTargetY: rendererBridge.targetY

    readonly property real localTargetX: windowTargetX - Screen.virtualX
    readonly property real localTargetY: (placeBelow ? rendererBridge.targetBelowY : windowTargetY) - Screen.virtualY
    readonly property real rawTargetBelowY: rendererBridge.targetBelowY - Screen.virtualY

    // Set LayerShell margins instantly based on raw values
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorLeft
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
    LayerShell.Window.activateOnShow: false
    LayerShell.Window.wantsToBeOnActiveScreen: true
    
    LayerShell.Window.margins.left: Math.max(8, Math.min(Screen.width - width - 8, Math.round(localTargetX - 100)))
    LayerShell.Window.margins.top: Math.max(8, Math.min(Screen.height - height - 8, Math.round(localTargetY - 20)))
    LayerShell.Window.margins.right: 0
    LayerShell.Window.margins.bottom: 0

    // Compute the actual clamped coordinates of the window relative to the screen virtual coordinates
    readonly property real clampedWindowX: Math.max(8, Math.min(Screen.width - width - 8, Math.round(localTargetX - 100))) + Screen.virtualX
    readonly property real clampedWindowY: Math.max(8, Math.min(Screen.height - height - 8, Math.round(localTargetY - 20))) + Screen.virtualY

    // Smoothly animated target coordinates for internal content movement
    property real smoothX: windowTargetX
    property real smoothY: placeBelow ? rendererBridge.targetBelowY : windowTargetY

    Behavior on smoothX { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on smoothY { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    Item {
        id: visualContent
        width: (isFlashing ? flashRow.implicitWidth : row.implicitWidth) + 24
        height: 58

        // Coordinate translation relative to the window
        x: smoothX - clampedWindowX
        y: smoothY - clampedWindowY

        Rectangle {
            id: bg
            anchors.fill: parent
            anchors.margins: 2
            radius: height / 2
            color: isFlashing ? "#f0092c46" : "#d9090d16"
            border.width: isFlashing ? 1.2 : 0.7
            border.color: isFlashing ? "#aa3dfcff" : "#22ffffff"

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }

            // Мягкое внутреннее свечение
            Rectangle {
                anchors.fill: parent
                anchors.margins: 0.5
                radius: parent.radius
                color: "transparent"
                border.width: 0.5
                border.color: "#0dffffff"
            }
        }

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
            visible: !isFlashing && rendererBridge.selectedIndex >= 0 && rendererBridge.selectedIndex < repeater.count
            property var targetItem: visible ? repeater.itemAt(rendererBridge.selectedIndex) : null

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
            visible: !isFlashing
            Repeater {
                id: repeater
                model: rendererBridge.candidates
                CandidateCapsule {
                    selected: index === rendererBridge.selectedIndex
                    onChosen: function(candidateIndex) { rendererBridge.chooseCandidate(candidateIndex) }
                }
            }
        }

        Row {
            id: flashRow
            anchors.centerIn: parent
            spacing: 8
            visible: isFlashing
            padding: 10

            Text {
                text: "✨"
                color: "#ff3dfcff"
                font.pixelSize: 15
                font.family: smartTypeFontFamily
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: rendererBridge.correction
                color: "#ffffffff"
                font.pixelSize: 15
                font.bold: true
                font.family: smartTypeFontFamily
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    function refreshBlur() {
        const areas = []
        if (isFlashing) {
            areas.push({
                x: visualContent.x + flashRow.x + 4,
                y: visualContent.y + 4,
                width: flashRow.width - 8,
                height: 50,
                radius: 25
            })
        } else {
            for (let i = 0; i < repeater.count; ++i) {
                const item = repeater.itemAt(i)
                if (item) areas.push({
                    x: visualContent.x + row.x + item.x + 4,
                    y: visualContent.y + row.y + item.y + 4,
                    width: item.width - 8,
                    height: item.height - 8,
                    radius: (item.height - 8) / 2
                })
            }
        }
        blurController.updateBlur(production, areas)
    }

    onVisibleChanged: {
        console.info("[LOG " + Qt.formatTime(new Date(), "hh:mm:ss.zzz") + "] QML Window visible=" + visible + " width=" + width + " height=" + height)
        if (visible) blurRefreshTicks = 12
    }
    onWidthChanged: blurRefreshTicks = 12
    onRawTargetBelowYChanged: {
        if (placeBelow && (rawTargetBelowY + height > Screen.height - 8)) {
            placeBelow = false
        } else if (!placeBelow && (rawTargetBelowY + height <= Screen.height - 8)) {
            placeBelow = true
        }
    }
    Connections {
        target: rendererBridge
        function onPanelChanged() {
            production.blurRefreshTicks = 12
            console.info("[LOG " + Qt.formatTime(new Date(), "hh:mm:ss.zzz") + "] QML onPanelChanged: flashId=" + rendererBridge.flashId + " correction=" + rendererBridge.correction)
            if (rendererBridge.flashId > 0) {
                flashAnim.restart()
            }
        }
    }
    Timer {
        interval: 40
        running: production.visible && production.blurRefreshTicks > 0
        repeat: true
        onTriggered: {
            production.refreshBlur()
            production.blurRefreshTicks--
        }
    }
    SequentialAnimation {
        id: flashAnim
        
        ParallelAnimation {
            NumberAnimation { target: visualContent; property: "scale"; from: 0.85; to: 1.05; duration: 120; easing.type: Easing.OutQuad }
            NumberAnimation { target: visualContent; property: "opacity"; from: 0.0; to: 1.0; duration: 100 }
        }
        PauseAnimation { duration: 250 }
        ParallelAnimation {
            NumberAnimation { target: visualContent; property: "scale"; to: 0.95; duration: 230; easing.type: Easing.InQuad }
            NumberAnimation { target: visualContent; property: "opacity"; to: 0.0; duration: 230 }
        }
        
        onRunningChanged: {
            console.info("[LOG " + Qt.formatTime(new Date(), "hh:mm:ss.zzz") + "] QML flashAnim running=" + running)
            if (!running) {
                visualContent.scale = 1.0
                visualContent.opacity = 1.0
            }
        }
    }
 
    Repeater {
        model: rendererBridge.flashes
        delegate: Item {
            id: flashDelegate
            readonly property int flashId: modelData.id
            readonly property real flashX: modelData.x
            readonly property real flashY: modelData.y
            readonly property real flashHeight: modelData.height
            readonly property string flashWord: modelData.word
 
            x: rendererBridge.windowX + flashX - clampedWindowX - measuredText.implicitWidth
            y: rendererBridge.windowY + flashY - clampedWindowY
            width: measuredText.implicitWidth > 0 ? measuredText.implicitWidth : 15
            height: flashHeight
 
            Text {
                id: measuredText
                text: flashWord
                font.pixelSize: 15
                font.family: smartTypeFontFamily
                visible: false
            }
 
            Rectangle {
                id: sweepBg
                anchors.fill: parent
                radius: height / 2
                color: "#2a3dfcff"
                opacity: 0
 
                Rectangle {
                    id: sweepGlint
                    x: -50
                    y: 0
                    width: 50
                    height: parent.height
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#00ffffff" }
                        GradientStop { position: 0.5; color: "#77ffffff" }
                        GradientStop { position: 1.0; color: "#00ffffff" }
                    }
                }
            }
 
            SequentialAnimation {
                running: true
 
                ParallelAnimation {
                    NumberAnimation { target: sweepBg; property: "opacity"; from: 0; to: 1; duration: 30 }
                }
 
                ParallelAnimation {
                    NumberAnimation { target: sweepGlint; property: "x"; from: -50; to: flashDelegate.width; duration: 80 }
                }
 
                ParallelAnimation {
                    NumberAnimation { target: sweepBg; property: "opacity"; to: 0; duration: 40 }
                }
 
                onFinished: {
                    rendererBridge.removeFlash(flashId)
                }
            }
        }
    }
}
