// AppButton.qml - 对齐 .btn 系列：默认 / primary / tiny / danger
// 用法: AppButton { text: "刷新"; variant: "primary" }
import QtQuick 2.15
import ".."

Item {
    id: root

    property string text: ""
    property string variant: "default"   // default | primary | tiny | danger
    property bool enabled: true
    property bool hoverAccent: false     // 默认按钮 hover 变蓝（.bfl/.settings-btn）
    property bool bold: false            // font-weight: 600（设置入口）
    property alias hovered: area.containsMouse
    signal clicked()

    implicitWidth: label.implicitWidth + (variant === "tiny" ? 20 : 28)
    implicitHeight: variant === "tiny" ? 24 : 30
    opacity: enabled ? 1.0 : (variant === "tiny" ? 0.6 : 0.5)

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radiusS
        border.width: 1

        color: {
            if (root.variant === "primary")
                return area.containsMouse && root.enabled ? Theme.accentDark : Theme.accent
            if (root.variant === "danger" && area.containsMouse && root.enabled)
                return Theme.dangerBg
            return Theme.panelBg
        }
        border.color: {
            if (root.variant === "primary") return Theme.accent
            if (root.variant === "danger")  return Theme.dangerBorder
            if (area.containsMouse && root.enabled)
                return root.hoverAccent ? Theme.accent : "#aaaaaa"
            return Theme.inputBorder
        }
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: root.variant === "tiny" ? Theme.fontM : Theme.fontBase
        font.family: Theme.fontUi
        font.weight: root.bold ? Font.DemiBold : Font.Normal
        color: {
            if (root.variant === "primary") return "#ffffff"
            if (root.variant === "danger")  return Theme.danger
            if (area.containsMouse && root.enabled && root.hoverAccent) return Theme.accent
            return Theme.textPrimary
        }
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        enabled: root.enabled
        // 对齐 QWidget：点击按钮即把焦点移出输入框（QML 默认点击不改变焦点）
        onPressed: root.forceActiveFocus()
        onClicked: root.clicked()
    }
}
