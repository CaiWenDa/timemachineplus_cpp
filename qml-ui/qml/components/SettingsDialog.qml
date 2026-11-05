// SettingsDialog.qml - 对齐 SettingsModal.vue
// 模态：源路径/目标路径管理，表单验证、notice 条（3.5s 自动消失）、Esc/遮罩关闭
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Popup {
    id: root

    property var sources: []
    property var targets: []
    property string sourceInput: ""
    property string targetInput: ""
    property string sourceError: ""
    property string targetError: ""
    property string notice: ""
    property string noticeType: "ok"   // ok | err

    signal closed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    // Esc 或点击遮罩关闭（对齐 onKeydown Escape + @click.self）
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    width: Math.min(Theme.modalWidth, (Overlay.overlay ? Overlay.overlay.width : 800) * 0.92)
    // 高度取内容自然高度，上限 86% 窗口高（超出部分由内部 Flickable 滚动，对齐 max-height: 86vh）
    height: Math.min(modalColumn.implicitHeight, (Overlay.overlay ? Overlay.overlay.height : 700) * 0.86)

    onAboutToShow: {
        notice = ""
        sourceError = ""; targetError = ""
        backend.loadConfig()
    }
    onClosed: root.closed()

    Connections {
        target: backend
        function onConfigReady(srcs, tgts) {
            root.sources = srcs
            root.targets = tgts
        }
        function onOpNotice(type, text) { root.showNotice(type, text) }
    }

    function showNotice(type, text) {
        noticeType = type
        notice = text
        noticeTimer.restart()
    }
    Timer {
        id: noticeTimer
        interval: 3500
        onTriggered: root.notice = ""
    }

    function humanSize(bytes) {
        if (bytes === null || bytes === undefined || bytes < 0) return ""
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }

    function submitSource() {
        var p = sourceInput.trim()
        if (!p) { sourceError = "路径不能为空"; return }
        sourceError = ""
        sourceInput = ""
        backend.addSource(p)
    }
    function submitTarget() {
        var p = targetInput.trim()
        if (!p) { targetError = "路径不能为空"; return }
        targetError = ""
        targetInput = ""
        backend.addTarget(p)
    }

    background: Rectangle {
        radius: Theme.radiusXL
        color: Theme.panelBg
        border.color: Theme.borderSoft
    }

    Overlay.modal: Rectangle { color: "#59000000" }   // rgba(0,0,0,.35)

    contentItem: Flickable {
        id: modalFlick
        contentWidth: width
        contentHeight: modalColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: modalColumn
            width: modalFlick.width
            spacing: 0

            // ---- 头部：⚙️ 设置 + ✕ ----
            Item {
                width: parent.width; height: 56
                Text {
                    anchors { left: parent.left; leftMargin: 24; verticalCenter: parent.verticalCenter }
                    text: "⚙️ 设置"
                    font.pixelSize: 17; font.weight: Font.DemiBold
                    font.family: Theme.fontUi; color: Theme.textPrimary
                }
                Text {
                    anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                    text: "✕"
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                    color: closeArea.containsMouse ? "#333333" : "#999999"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        anchors.margins: -10
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.borderSoft }
            }

            // ---- notice 条（3.5s 自动消失 + 淡入淡出） ----
            Rectangle {
                visible: opacity > 0
                opacity: root.notice.length > 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300 } }
                width: parent.width - 40
                anchors.horizontalCenter: parent.horizontalCenter
                height: visible ? (noticeText.implicitHeight + 18) : 0
                Behavior on height { NumberAnimation { duration: 200 } }
                radius: Theme.radiusS
                color: root.noticeType === "ok" ? Theme.noticeOkBg : Theme.noticeErrBg
                border.width: 1
                border.color: root.noticeType === "ok" ? Theme.ok : Theme.danger
                Text {
                    id: noticeText
                    anchors { fill: parent; margins: 9 }
                    text: root.notice
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                    color: root.noticeType === "ok" ? Theme.ok : Theme.danger
                    wrapMode: Text.WordWrap
                }
            }

            // ==================== 备份源路径 ====================
            SettingsSection {
                width: parent.width
                icon: "📂"
                title: "备份源路径"
                description: "这些目录中的文件会被定期备份"
                placeholder: "例如 D:\\Photos 或 C:\\Users\\Alice\\Documents"
                inputText: root.sourceInput
                errorText: root.sourceError
                listModel: root.sources
                isTarget: false
                onInputChanged: function(t) { root.sourceInput = t }
                onSubmit: root.submitSource()
                onRemove: function(id) { backend.removeSource(id) }
                humanSizeFunc: root.humanSize
            }

            Rectangle { width: parent.width - 40; anchors.horizontalCenter: parent.horizontalCenter; height: 1; color: Theme.border }

            // ==================== 备份目标路径 ====================
            SettingsSection {
                width: parent.width
                icon: "💾"
                title: "备份目标路径"
                description: "备份文件存储的磁盘，新备份会优先写入剩余空间最大的目标"
                placeholder: "例如 E:\\Backups"
                inputText: root.targetInput
                errorText: root.targetError
                listModel: root.targets
                isTarget: true
                onInputChanged: function(t) { root.targetInput = t }
                onSubmit: root.submitTarget()
                onRemove: function(id) { backend.removeTarget(id) }
                humanSizeFunc: root.humanSize
            }

            // ---- 底部 ----
            Item {
                width: parent.width; height: 68
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                Text {
                    anchors { left: parent.left; leftMargin: 24; verticalCenter: parent.verticalCenter }
                    text: "配置变更即时生效，无需重启应用"
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textSecondary
                }
                AppButton {
                    anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                    text: "完成"
                    variant: "primary"
                    onClicked: root.close()
                }
            }
        }
    }

    // 打开动画：opacity + scale（对齐 fade/scale 过渡）
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 200 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 }
    }
}
