// SearchResultsPane.qml - 对齐 SearchResults.vue
// 关键词高亮 + 版本徽章 + 点击跨版本定位
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Item {
    id: root

    property string query: ""
    property var results: []
    property bool searching: false
    property bool searched: false

    signal locate(int backupId, string filepath)

    function doSearch(kw) {
        query = kw
        searching = true
        searched = true
        backend.search(kw)
    }

    Connections {
        target: backend
        function onSearchReady(keyword, list) {
            // 只接受当前 query 的结果，防止旧请求覆盖新结果（对齐版本比较）
            if (keyword !== root.query) return
            root.results = list
            root.searching = false
        }
    }

    function basename(p) {
        var s = String(p)
        var i = Math.max(s.lastIndexOf("/"), s.lastIndexOf("\\"))
        return i < 0 ? s : s.substring(i + 1)
    }
    function humanSize(bytes) {
        if (bytes === null || bytes === undefined) return ""
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }
    // 生成 HTML 高亮片段（等价于 highlight() 的 <mark>）
    function highlightHtml(name, kw) {
        if (!kw) return escapeHtml(name)
        var lower = name.toLowerCase(), q = kw.toLowerCase()
        var out = "", i = 0, idx
        while ((idx = lower.indexOf(q, i)) !== -1) {
            out += escapeHtml(name.substring(i, idx))
            out += "<font color='" + Theme.textPrimary + "' bgcolor='" + Theme.highlightBorder + "'>"
                 + escapeHtml(name.substring(idx, idx + kw.length)) + "</font>"
            i = idx + kw.length
        }
        out += escapeHtml(name.substring(i))
        return out
    }
    function escapeHtml(s) {
        return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // 头部：🔍「kw」的搜索结果 + N 条匹配 / 搜索中
        Item {
            width: parent.width; height: 34
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "🔍「" + root.query + "」的搜索结果"
                font.pixelSize: Theme.fontL; font.weight: Font.DemiBold
                font.family: Theme.fontUi; color: Theme.textPrimary
            }
            Text {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                text: root.searching ? "搜索中…" : root.results.length + " 条匹配"
                font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                color: Theme.textSecondary
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1; color: Theme.borderSoft
            }
        }
        Item { width: 1; height: 10 }

        // 空态
        Text {
            visible: !root.searching && root.results.length === 0
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 40
            text: "在所有备份版本中没有找到匹配「" + root.query + "」的文件。"
            font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
            color: Theme.textSecondary
        }

        // 结果列表
        ListView {
            width: parent.width
            height: parent.height - 44
            clip: true
            spacing: 6
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            reuseItems: true
            model: root.results

            delegate: Rectangle {
                width: ListView.view.width
                height: resultColumn.height + 20
                radius: Theme.radiusM
                color: resultArea.containsMouse ? Theme.itemHover : Theme.itemBg
                Behavior on color { ColorAnimation { duration: 150 } }

                Column {
                    id: resultColumn
                    anchors { left: parent.left; leftMargin: 12; top: parent.top; topMargin: 10 }
                    width: parent.width - 24 - badgeRow.width - 12
                    spacing: 4
                    Text {
                        text: root.highlightHtml("📄 " + root.basename(modelData.filepath), root.query)
                        textFormat: Text.RichText
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
                }

                Row {
                    id: badgeRow
                    anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 8
                    // 版本徽章：🕒 MM-dd hh:mm（蓝字浅蓝底圆角胶囊）
                    Rectangle {
                        height: 20
                        width: badgeText.implicitWidth + 16
                        radius: 10
                        color: Theme.accentSoft
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: "🕒 " + (modelData.backupBeginTime || "").substring(5, 16)
                            font.pixelSize: Theme.fontM; font.weight: Font.DemiBold
                            font.family: Theme.fontUi; color: Theme.accent
                        }
                    }
                    Text {
                        text: root.humanSize(modelData.filesize)
                        font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                        color: Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: resultArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.locate(modelData.backupId, modelData.filepath)
                }
            }
        }
    }
}
