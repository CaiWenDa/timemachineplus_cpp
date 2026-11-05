// ConfirmDialog.qml - 通用确认模态（替代浏览器 confirm()，视觉对齐应用整体风格）
// 用法:
//   ConfirmDialog { id: dlg; title: "..."; message: "..."; onAccepted: ... }
//   confirmDlg.openWith("标题", "正文", function(){ ... })
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Popup {
    id: root

    property string title: ""
    property string message: ""
    property string okText: "确定"
    property string cancelText: "取消"
    property var onAcceptedCallback: null

    // 便捷入口：带回调打开
    function openWith(t, msg, cb) {
        title = t; message = msg; onAcceptedCallback = cb
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width * 0.9 : 420)

    background: Rectangle {
        radius: Theme.radiusXL
        color: Theme.panelBg
        border.color: Theme.borderSoft
    }

    Overlay.modal: Rectangle { color: "#59000000" }   // rgba(0,0,0,.35)

    contentItem: Column {
        spacing: 0

        // 标题栏：容器用 Item（不含边框），仅保留底部分隔线
        // 注意：不要给无 radius 的矩形只设 border.color —— Qt 的 border.width 默认为 1，
        // 会画出直角矩形边线，压在圆角窗口顶部形成"直角细线残留"
        Item {
            width: parent.width; height: 48
            Text {
                anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
                text: root.title
                font.pixelSize: Theme.fontXL; font.weight: Font.DemiBold
                font.family: Theme.fontUi; color: Theme.textPrimary
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.borderSoft }
        }

        // 正文
        Text {
            width: parent.width - 40
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 16; bottomPadding: 20
            text: root.message
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
            color: Theme.textPrimary
            lineHeight: 1.4
        }

        // 按钮行
        Item {
            width: parent.width; height: 56
            Row {
                anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                spacing: 10
                AppButton {
                    text: root.cancelText
                    onClicked: root.close()
                }
                AppButton {
                    text: root.okText
                    variant: "primary"
                    onClicked: {
                        root.close()
                        if (root.onAcceptedCallback) root.onAcceptedCallback()
                    }
                }
            }
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }
}
