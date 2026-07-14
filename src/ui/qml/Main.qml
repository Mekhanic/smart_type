import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: root
    width: 1080
    height: 650
    visible: true
    title: "SmartType UI — isolated prototype"
    color: "#070a10"

    property var candidates: ["красиво", "красивый", "красивое", "красивую"]
    property int selectedIndex: 0
    property bool panelBelow: false
    readonly property bool screenshotMode: Qt.application.arguments.indexOf("--screenshot-mode") >= 0

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#080d16" }
            GradientStop { position: 0.54; color: "#111522" }
            GradientStop { position: 1; color: "#070910" }
        }
    }

    Rectangle {
        id: cyanOrbSource
        x: 86; y: 68; width: 310; height: 310; radius: 155; visible: false
        gradient: Gradient {
            GradientStop { position: 0; color: "#6bc5e8" }
            GradientStop { position: 0.48; color: "#25658c" }
            GradientStop { position: 1; color: "#152a4b" }
        }
    }
    MultiEffect {
        anchors.fill: cyanOrbSource; source: cyanOrbSource
        blurEnabled: true; blur: 0.42; blurMax: 38; opacity: 0.54
        shadowEnabled: true; shadowColor: "#5535a9ff"; shadowBlur: 1.0
    }

    Rectangle {
        id: violetOrbSource
        x: 705; y: 58; width: 248; height: 248; radius: 124; visible: false
        gradient: Gradient {
            GradientStop { position: 0; color: "#b27ad7" }
            GradientStop { position: 0.55; color: "#644c98" }
            GradientStop { position: 1; color: "#32254f" }
        }
    }
    MultiEffect {
        anchors.fill: violetOrbSource; source: violetOrbSource
        blurEnabled: true; blur: 0.48; blurMax: 38; opacity: 0.48
        shadowEnabled: true; shadowColor: "#554f55ff"; shadowBlur: 1.0
    }

    Rectangle {
        id: ambientSource
        x: 350; y: 278; width: 420; height: 160; radius: 80; visible: false
        gradient: Gradient {
            GradientStop { position: 0; color: "#2d7f88" }
            GradientStop { position: 0.54; color: "#344e88" }
            GradientStop { position: 1; color: "#654071" }
        }
    }
    MultiEffect {
        anchors.fill: ambientSource; source: ambientSource
        blurEnabled: true; blur: 0.72; blurMax: 48; opacity: 0.38
    }

    Label {
        anchors { top: parent.top; left: parent.left; margins: 28 }
        text: "Изолированный прототип • набирайте текст • ← → и Tab выбирают"
        color: "#78879d"
        font.family: smartTypeFontFamily
        font.pixelSize: 14
    }

    Rectangle {
        id: composer
        x: 54
        y: root.height - 126
        width: root.width - 108
        height: 76
        radius: 26
        color: "#d9101521"
        border.color: "#24ffffff"
        border.width: 1

        TextField {
            id: editor
            anchors { fill: parent; leftMargin: 24; rightMargin: 24 }
            text: "можешь теперь сделать краси"
            color: "#f3f6fb"
            selectionColor: "#5066a9ff"
            selectedTextColor: "white"
            font.family: smartTypeFontFamily
            font.pixelSize: 21
            font.weight: Font.Medium
            background: Item {}
            focus: true

            Keys.onLeftPressed: event => {
                if (event.modifiers === Qt.NoModifier) {
                    root.selectedIndex = (root.selectedIndex + root.candidates.length - 1) % root.candidates.length
                    event.accepted = true
                }
            }
            Keys.onRightPressed: event => {
                if (event.modifiers === Qt.NoModifier) {
                    root.selectedIndex = (root.selectedIndex + 1) % root.candidates.length
                    event.accepted = true
                }
            }
            Keys.onTabPressed: event => {
                root.acceptCandidate(root.selectedIndex)
                event.accepted = true
            }
        }
    }

    TextMetrics {
        id: typedMetrics
        font: editor.font
        text: {
            const prefix = editor.text.slice(0, editor.cursorPosition)
            const match = prefix.match(/[А-Яа-яЁёA-Za-z-]+$/)
            return match ? match[0] : ""
        }
    }

    function updatePanelPosition() {
        const caret = editor.cursorRectangle
        // CandidatePanel is an xdg_popup transient to this window. On Wayland
        // its coordinates are relative to the parent surface, unlike a Tool
        // window whose global position is intentionally controlled by KWin.
        const originX = screenshotMode ? root.x : 0
        const originY = screenshotMode ? root.y : 0
        const caretX = originX + composer.x + editor.x + caret.x
        const caretY = originY + composer.y + editor.y + caret.y
        const wordStartX = caretX - typedMetrics.width
        const desired = wordStartX + Math.min(typedMetrics.width * 0.42, 72)
        const edge = 12
        candidatePanel.desiredX = Math.max(edge,
            Math.min(desired, originX + root.width - candidatePanel.width - edge))
        const above = caretY - candidatePanel.height - 15
        const below = caretY + caret.height + 15
        // Hysteresis prevents a panel near the top edge from alternating
        // above/below while glyph metrics change during composition.
        if (!panelBelow && above < edge) panelBelow = true
        else if (panelBelow && above > edge + 44) panelBelow = false
        candidatePanel.desiredY = panelBelow ? below : above
    }

    function acceptCandidate(index) {
        const prefix = editor.text.slice(0, editor.cursorPosition)
        const match = prefix.match(/[А-Яа-яЁёA-Za-z-]+$/)
        if (!match) return
        const start = editor.cursorPosition - match[0].length
        editor.remove(start, editor.cursorPosition)
        editor.insert(start, root.candidates[index])
        editor.cursorPosition = start + root.candidates[index].length
    }

    CandidatePanel {
        id: candidatePanel
        candidateModel: root.candidates
        selectedIndex: root.selectedIndex
        screenshotMode: root.screenshotMode
        onCandidateChosen: index => {
            root.selectedIndex = index
            root.acceptCandidate(index)
            editor.forceActiveFocus()
        }
    }

    Connections {
        target: editor
        function onCursorRectangleChanged() { root.updatePanelPosition() }
        function onTextChanged() { root.updatePanelPosition() }
    }
    Connections {
        target: root
        function onXChanged() { root.updatePanelPosition() }
        function onYChanged() { root.updatePanelPosition() }
    }
    onActiveChanged: {
        if (active && !candidatePanel.visible) {
            candidatePanel.visible = true
            candidatePanel.scheduleBlurRefresh()
        }
    }
    Component.onCompleted: Qt.callLater(updatePanelPosition)
}
