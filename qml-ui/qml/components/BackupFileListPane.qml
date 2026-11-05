// BackupFileListPane.qml - 对齐 BackupFileList.vue
// 占位/加载/错误/空/列表五态 + 恢复按钮状态机（idle→restoring→ok|fail）+ 确认对话框
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Item {
    id: root

    property var backup: null            // 当前选中的时间轴节点
    property string highlightPath: ""    // 搜索定位后需要高亮的文件路径

    property var files: []
    property bool loading: false
    property string error: ""

    // restoreState: { historyId: "idle"|"restoring"|"ok"|"fail" }
    // restoreMsg:   { historyId: "..." }
    property var restoreState: ({})
    property var restoreMsg: ({})

    onBackupChanged: {
        restoreState = {}
        restoreMsg = {}
        if (backup === null || backup === undefined) {
            files = []
        } else {
            loading = true
            error = ""
            backend.loadBackupFiles(backup.id)
        }
    }

    onHighlightPathChanged: scrollToHighlight()

    Connections {
        target: backend
        function onBackupFilesReady(backupId, list) {
            if (!root.backup || root.backup.id !== backupId) return
            root.files = list
            root.loading = false
            root.error = ""
            Qt.callLater(root.scrollToHighlight)
        }
        function onRestoreResult(historyId, ok, msg) {
            var s = root.restoreState, m = root.restoreMsg
            s[historyId] = ok ? "ok" : "fail"
            m[historyId] = (ok ? "" : "❌ ") + msg
            if (ok) m[historyId] = "✅ " + msg
            root.restoreState = Object.assign({}, s)
            root.restoreMsg = Object.assign({}, m)
        }
    }

    function norm(p) { return String(p).replace(/\//g, "\\").toLowerCase() }
    function isHighlighted(f) {
        return highlightPath.length > 0 && norm(f.filepath) === norm(highlightPath)
    }
    function basename(p) {
        var s = String(p)
        var i = Math.max(s.lastIndexOf("/"), s.lastIndexOf("\\"))
        return i < 0 ? s : s.substring(i + 1)
    }
    function humanSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }
    function humanDate(ms) {
        return Qt.formatDateTime(new Date(ms), "yyyy/M/d hh:mm:ss")
    }
    function scrollToHighlight() {
        if (highlightPath.length === 0 || fileList.count === 0) return
        for (var i = 0; i < files.length; ++i) {
            if (isHighlighted(files[i])) {
                fileList.positionViewAtIndex(i, ListView.Center)
                break
            }
        }
    }

    ConfirmDialog { id: confirmDlg }

    Column {
        anchors.fill: parent

        // ========== 未选中占位态 ==========
        Item {
            visible: root.backup === null
            width: parent.width; height: parent.height
            Column {
                anchors.centerIn: parent
                spacing: 4
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "👈 点击左侧时间轴上的任意时间点"
                    font.pixelSize: Theme.fontXL; font.family: Theme.fontUi
                    color: Theme.textTertiary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "查看该备份版本中包含的所有文件"
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textMuted
                }
            }
        }

        // ========== 已选中 ==========
        Column {
            visible: root.backup !== null
            width: parent.width; height: parent.height
            spacing: 0

            // 头部：📦 时间的备份版本 + N 个文件
            Item {
                width: parent.width; height: 34
                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    text: "📦 " + (root.backup ? (root.backup.begintime || "未知时间") : "") + " 的备份版本"
                    font.pixelSize: Theme.fontL; font.weight: Font.DemiBold
                    font.family: Theme.fontUi; color: Theme.textPrimary
                }
                Text {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    text: root.files.length + " 个文件"
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textSecondary
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width; height: 1; color: Theme.borderSoft
                }
            }
            Item { width: 1; height: 10 }

            // 加载态
            Row {
                visible: root.loading
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 40
                spacing: 8
                AppSpinner { anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "正在加载文件列表…"
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // 错误态
            Text {
                visible: !root.loading && root.error.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 40
                text: root.error
                font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                color: Theme.danger
            }

            // 空态
            Text {
                visible: !root.loading && root.error.length === 0 && root.files.length === 0
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 40
                text: "该备份版本没有文件记录。"
                font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                color: Theme.textSecondary
            }

            // 列表
            ListView {
                id: fileList
                visible: !root.loading && root.error.length === 0 && root.files.length > 0
                width: parent.width
                height: parent.height - 44
                clip: true
                spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                reuseItems: true

                model: root.files

                delegate: Rectangle {
                    width: fileList.width
                    height: fileColumn.height + 20
                    radius: Theme.radiusM
                    color: root.isHighlighted(modelData)
                           ? Theme.highlightBg
                           : (fileArea.containsMouse ? Theme.itemHover : Theme.itemBg)
                    border.width: 1
                    border.color: root.isHighlighted(modelData) ? Theme.highlightBorder : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    Column {
                        id: fileColumn
                        anchors { left: parent.left; leftMargin: 12; top: parent.top; topMargin: 10 }
                        width: parent.width - 24 - restoreBtn.width - 12
                        spacing: 4

                        Text {
                            text: "📄 " + root.basename(modelData.filepath)
                            font.pixelSize: Theme.fontBase; font.weight: Font.DemiBold
                            font.family: Theme.fontUi; color: Theme.textPrimary
                            elide: Text.ElideMiddle; width: parent.width
                        }
                        Text {
                            text: modelData.filepath
                            font.pixelSize: Theme.fontS; font.family: Theme.fontUi
                            color: Theme.textTertiary
                            elide: Text.ElideRight; width: parent.width
                        }
                        Text {
                            text: root.humanSize(modelData.filesize) + " · 修改于 " + root.humanDate(modelData.motifytime)
                            font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                            color: "#666666"
                        }
                        // 恢复结果消息（ok 绿 / fail 红）
                        Text {
                            visible: (root.restoreState[modelData.historyId] === "ok"
                                      || root.restoreState[modelData.historyId] === "fail")
                            text: root.restoreMsg[modelData.historyId] || ""
                            font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                            color: root.restoreState[modelData.historyId] === "ok" ? Theme.ok : Theme.danger
                            width: parent.width; wrapMode: Text.WrapAnywhere
                        }
                    }

                    MouseArea {
                        id: fileArea
                        anchors.fill: parent
                        hoverEnabled: true
                        // 列表项本身无点击行为，仅提供 hover 背景；同时移出搜索框焦点
                        onClicked: { root.forceActiveFocus(); mouse.accepted = false }
                    }

                    // 恢复按钮
                    Row {
                        id: restoreBtn
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        spacing: 5
                        AppSpinner {
                            visible: root.restoreState[modelData.historyId] === "restoring"
                            size: 10; borderWidth: 1.5
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        AppButton {
                            text: root.restoreState[modelData.historyId] === "restoring" ? "恢复中…" : "↩ 恢复"
                            enabled: root.restoreState[modelData.historyId] !== "restoring"
                            hoverAccent: true
                            onClicked: {
                                var f = modelData
                                confirmDlg.openWith(
                                    "恢复文件",
                                    "将该文件从当前备份版本恢复到原位置？\n\n文件：" + root.basename(f.filepath)
                                    + "\n目标：" + f.filepath + "\n\n同名现有文件将被覆盖。",
                                    function() {
                                        var s = root.restoreState
                                        s[f.historyId] = "restoring"
                                        root.restoreState = Object.assign({}, s)
                                        backend.restore(f.historyId, f.filepath)
                                    })
                            }
                        }
                    }

                }
            }
        }
    }
}
