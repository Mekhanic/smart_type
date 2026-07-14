import QtQuick
import QtQuick.Effects

Item {
    id: capsule
    required property int index
    required property var modelData
    readonly property string label: String(modelData)
    readonly property int candidateIndex: index
    property bool selected: false
    signal chosen(int index)

    width: Math.max(92, text.implicitWidth + 36)
    height: selected ? 50 : 46
    y: selected ? 1 : 4
    scale: selected ? 1.028 : 1.0
    opacity: 0
    transformOrigin: Item.Center

    Behavior on scale { SpringAnimation { spring: 4.8; damping: 0.42; mass: 0.68 } }
    Behavior on y { NumberAnimation { duration: 135; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 135; easing.type: Easing.OutCubic } }

    Rectangle {
        id: shadowSource
        anchors.fill: glass
        radius: height / 2
        color: "#76000000"
        visible: false
    }

    MultiEffect {
        source: shadowSource
        anchors.fill: shadowSource
        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: "#a8000000"
        shadowBlur: 0.62
        shadowVerticalOffset: 7
        shadowHorizontalOffset: 0
        opacity: capsule.opacity
    }

    Rectangle {
        id: glass
        anchors.fill: parent
        anchors.margins: 4
        radius: height / 2
        color: selected ? "#00ffffff" : "#0effffff"
        border.width: selected ? 0.0 : 0.7
        border.color: selected ? "#00ffffff" : "#1affffff"
        clip: true

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: capsule.selected ? "#002dd7ff" : "#0fffffff" }
                GradientStop { position: 0.58; color: capsule.selected ? "#006652ff" : "#05ffffff" }
                GradientStop { position: 1.0; color: capsule.selected ? "#00d65cff" : "#01ffffff" }
            }
            opacity: capsule.selected ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1.0
            radius: height / 2
            color: "transparent"
            border.width: 0.5
            border.color: capsule.selected ? "#00ffffff" : "#18ffffff"
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        Rectangle {
            id: topGlint
            x: 12
            y: 1.5
            width: parent.width - 24
            height: Math.max(6, parent.height * 0.25)
            radius: height / 2
            gradient: Gradient {
                GradientStop { position: 0; color: capsule.selected ? "#00ffffff" : "#38ffffff" }
                GradientStop { position: 1; color: "#00ffffff" }
            }
            opacity: capsule.selected ? 0.0 : 0.60
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        Rectangle {
            id: travelingShine
            width: 30
            height: parent.height * 1.6
            y: -parent.height * 0.3
            x: -width - 20
            rotation: 16
            color: "#00ffffff"
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#00ffffff" }
                GradientStop { position: 0.5; color: "#14ffffff" }
                GradientStop { position: 1; color: "#00ffffff" }
            }
            SequentialAnimation on x {
                running: !capsule.selected // Only run shine when not selected
                loops: Animation.Infinite
                PauseAnimation { duration: 2500 + capsule.candidateIndex * 350 }
                NumberAnimation { from: -60; to: glass.width + 30; duration: 2000; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 4500 }
            }
        }

        Text {
            id: text
            anchors.centerIn: parent
            text: capsule.label
            color: capsule.selected ? "#ffffff" : "#c5d1e2"
            Behavior on color { ColorAnimation { duration: 150 } }
            font.family: smartTypeFontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }
    }

    TapHandler { onTapped: capsule.chosen(capsule.candidateIndex) }

    SequentialAnimation {
        running: true
        PauseAnimation { duration: 35 * capsule.candidateIndex }
        ParallelAnimation {
            NumberAnimation { target: capsule; property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: capsule; property: "scale"; from: 0.88; to: capsule.selected ? 1.028 : 1.0; duration: 280; easing.type: Easing.OutBack }
        }
    }
}
