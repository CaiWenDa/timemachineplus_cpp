// TimelinePane.qml - 对齐 Timeline.vue
// 刷新/立即备份按钮行 + 按日期分组的时间点列表（竖线 + 圆点 + 卡片）
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Item {
    id: root

    // 外部控制选中（搜索结果跨版本定位时传入）
    property var selectedId: null
    signal selectBackup(var backup)

    property var backups: []
    property bool loading: true

    function reload() {
        loading = true
        backend.refreshBackups()
    }

    function humanSize(bytes) {
        if (bytes === null || bytes === undefined) return "—"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }

    // 按日期分组：YYYY-MM-DD → [items]（输入已按 id desc 排序，保持顺序）
    function groupByDay(list) {
        var map = {}, order = []
        for (var i = 0; i < list.length; ++i) {
            var b = list[i]
            var day = (b.begintime || "未知").substring(0, 10)
            if (!(day in map)) { map[day] = []; order.push(day) }
            map[day].push(b)
        }
        return order.map(function(d) { return { day: d, items: map[d] } })
    }

    Connections {
        target: backend
        function onBackupsReady(list) {
            root.backups = list
            root.loading = false
        }
    }

    // 外部 selectedId 变化 → 同步高亮并滚动到可见
    onSelectedIdChanged: {
        if (selectedId === null || selectedId === undefined) return
        scrollToSelected()
    }
    function scrollToSelected() {
        // 找到 delegate 位置后让 ListView 滚过去（平滑）
        for (var g = 0; g < groupModel.count; ++g) {
            // 简化：整体滚动通过 ListView 的 positionViewAtIndex 难以跨分组，
            // 使用 eachDelegate 手动计算 y 偏移
        }
        flickScroll.ensureVisibleForId(root.selectedId)
    }

    Component.onCompleted: reload()

    // 分组模型：直接用 JS 数组作 Repeater model（delegate 用 modelData 访问），
    // 不能用 ListModel 嵌套对象数组——ListElement 不支持，会导致 delegate 取不到字段
    property var groupedData: []
    onBackupsChanged: groupedData = groupByDay(backups)

    Column {
        anchors.fill: parent

        // ---- 控制条：刷新 / 立即备份 / N 个备份点 ----
        Item {
            width: parent.width
            height: 48
            Row {
                anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
                spacing: 10
                AppButton {
                    text: root.loading ? "加载中…" : "刷新"
                    variant: "primary"
                    enabled: !root.loading
                    onClicked: root.reload()
                }
                AppButton {
                    text: "📸 立即备份"
                    onClicked: {
                        backend.triggerBackup()
                        // 与 Electron 版一致：触发后立即刷新（备份后台进行）
                        root.reload()
                    }
                }
            }
            Text {
                anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                text: root.backups.length + " 个备份点"
                font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                color: Theme.textSecondary
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1; color: Theme.borderSoft
            }
        }

        // ---- 空状态 ----
        Item {
            visible: root.backups.length === 0 && !root.loading
            width: parent.width; height: parent.height - 48
            Column {
                anchors.centerIn: parent
                spacing: 4
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "还没有备份历史。"
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                    color: Theme.textTertiary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "添加源路径后点击「立即备份」开始吧。"
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textTertiary
                }
            }
        }

        // ---- 时间轴滚动区 ----
        Flickable {
            id: flickScroll
            visible: root.backups.length > 0 || root.loading
            width: parent.width
            height: parent.height - 48
            contentWidth: width
            contentHeight: timelineColumn.height + 24
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            // 搜索定位：把指定 backupId 的卡片滚到可视区（对齐 scrollIntoView block:nearest）
            function ensureVisibleForId(id) {
                if (id === null || id === undefined) return
                var y = timelineColumn.yOfBackup(id)
                if (y < 0) return
                var top = contentY, bottom = contentY + height
                if (y < top) {
                    scrollAnim.to = y - 8; scrollAnim.restart()
                } else if (y + 44 > bottom) {
                    scrollAnim.to = y + 44 - height + 8; scrollAnim.restart()
                }
            }
            NumberAnimation { id: scrollAnim; target: flickScroll; property: "contentY"; duration: 300; easing.type: Easing.OutCubic }

            Column {
                id: timelineColumn
                x: 16; y: 12
                width: flickScroll.width - 32
                spacing: 16

                // 记录每个 backup 卡片的 y 坐标，供 ensureVisibleForId 查询
                property var yMap: ({})
                function yOfBackup(id) {
                    return (yMap && yMap[id] !== undefined) ? yMap[id] : -1
                }

                Repeater {
                    model: timelineColumn.parent ? root.groupedData : []
                    delegate: Column {
                        width: timelineColumn.width
                        spacing: 0
                        property string day: modelData.day
                        property var items: modelData.items

                        // 日期标签：11px 大写灰
                        Text {
                            text: day.toUpperCase()
                            font.pixelSize: Theme.fontS; font.weight: Font.DemiBold
                            font.family: Theme.fontUi
                            font.capitalization: Font.AllUppercase
                            color: Theme.textSecondary
                            leftPadding: 6; bottomPadding: 4; topPadding: 4
                        }

                        // 竖线 + 卡片列
                        Item {
                            width: parent.width
                            height: itemsColumn.height
                            // 左侧 2px 竖线（对齐 .tl-day-items border-left）
                            Rectangle {
                                x: 6; y: 0
                                width: 2; height: parent.height
                                color: Theme.border
                            }
                            Column {
                                id: itemsColumn
                                x: 26   // padding-left: 20px + 竖线宽
                                width: parent.width - 26
                                spacing: 8   // margin: 4px 0 → 卡片间距 8

                                Repeater {
                                    model: items
                                    delegate: Rectangle {
                                        id: dotCard
                                        width: itemsColumn.width
                                        height: 40
                                        radius: Theme.radiusM
                                        color: root.selectedId === modelData.id
                                               ? Theme.itemActive
                                               : (cardArea.containsMouse ? Theme.itemHover : Theme.itemBg)
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        // 圆点：12px 蓝圆 + 白边，中心对齐竖线中心(外层x=7) → 卡片内x = 7-26-6 = -25
                                        Rectangle {
                                            x: -25
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 12; height: 12; radius: 6
                                            color: modelData.endtime ? Theme.accent : Theme.warning
                                            border.width: 2; border.color: "#ffffff"
                                        }
                                        // 外发光（box-shadow: 0 0 0 2px）
                                        Rectangle {
                                            x: -27
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 16; height: 16; radius: 8
                                            color: "transparent"
                                            border.width: 2
                                            border.color: modelData.endtime ? Theme.accent : Theme.warning
                                            z: -1
                                        }

                                        Row {
                                            anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                                            spacing: 10
                                            Text {
                                                text: (modelData.begintime || "").substring(11, 16)
                                                font.pixelSize: Theme.fontBase; font.weight: Font.DemiBold
                                                font.family: Theme.fontUi; color: Theme.textPrimary
                                            }
                                            Text {
                                                text: (modelData.filecopycount || 0) + " 个文件"
                                                font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                                                color: "#666666"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                text: root.humanSize(modelData.datacopycount)
                                                font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                                                color: Theme.textTertiary
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        // 进行中徽章
                                        Rectangle {
                                            visible: !modelData.endtime
                                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                                            width: badgeText.implicitWidth + 16; height: 18
                                            radius: 9; color: Theme.warning
                                            Text {
                                                id: badgeText
                                                anchors.centerIn: parent
                                                text: "进行中"
                                                font.pixelSize: Theme.fontXS; font.family: Theme.fontUi
                                                color: "#ffffff"
                                            }
                                        }

                                        MouseArea {
                                            id: cardArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.forceActiveFocus()  // 点击卡片同时移出搜索框焦点
                                                root.selectedId = modelData.id
                                                root.selectBackup(modelData)
                                            }
                                        }

                                        // 记录 y 坐标供滚动定位
                                        Component.onCompleted: {
                                            var p = dotCard.mapToItem(timelineColumn, 0, 0)
                                            timelineColumn.yMap[modelData.id] = p.y
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
