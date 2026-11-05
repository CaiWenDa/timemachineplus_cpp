// Main.qml - 对齐 App.vue：顶栏（标题/搜索/状态/设置）+ 时间轴 + 内容区
// 窗口 960x600；响应式断点 860px（窄窗口单列堆叠） 
import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

ApplicationWindow {
    id: root
    width: 960
    height: 600
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Time Machine+"
    color: Theme.bg

    // ---- 全局状态（对齐 App.vue 的响应式数据） ----
    property string searchQuery: ""
    property bool searching: false
    property string healthStatus: "连接中…"
    property bool healthOk: false
    property var currentBackup: null
    property string highlightPath: ""

    // ---- 健康检查（启动时 + 重试按钮共用） ----
    function checkHealth() {
        healthStatus = "连接中…"
        healthOk = false
        backend.health()
    }

    Connections {
        target: backend
        function onHealthReady(ok, text) {
            root.healthOk = ok
            root.healthStatus = ok ? "后端已连接" : "后端未就绪"
        }
        function onOpNotice(type, text) {
            // 非设置场景的错误（如备份失败）通过全局通知展示
            if (!settingsDlg.opened) {
                toast.show(type, text)
            }
        }
    }

    Component.onCompleted: checkHealth()

    // 点击空白区域移除输入焦点（QML 默认不会自动清除 activeFocus，
    // 对齐 QWidget/浏览器行为：点到非交互区域时搜索框失焦）。
    // 用 TapHandler 而非 MouseArea：祖先级 TapHandler 与各面板的
    // Flickable 拖拽/子级 MouseArea 仲裁共存，不会拦截滚动和子控件点击；
    // 按钮、时间轴卡片等自身已处理焦点转移（AppButton.onPressed）。
    // 注意：焦点必须转移给一个具体 Item（Window.forceActiveFocus 不移除项焦点）。
    Item { id: focusSink }
    TapHandler {
        onTapped: focusSink.forceActiveFocus()
    }

    // ---- 搜索防抖：300ms（对齐 @input="onSearchInput"） ----
    Timer {
        id: searchDebounce
        interval: 300
        onTriggered: {
            var q = root.searchQuery.trim()
            if (q.length === 0) return
            root.searching = true
            searchPane.doSearch(q)
        }
    }

    // ---- 搜索定位（跨版本跳转） ----
    function handleLocate(backupId, filepath) {
        searchQuery = ""
        searchPane.results = []
        searchPane.searched = false
        timeline.selectedId = backupId
        highlightPath = filepath
        // 从时间轴列表中找到该备份并选中加载
        for (var i = 0; i < timeline.backups.length; ++i) {
            if (timeline.backups[i].id === backupId) {
                currentBackup = timeline.backups[i]
                break
            }
        }
    }

    // 全局通知条（复用设置页 notice 视觉风格，浮动在顶部）
    Rectangle {
        id: toast
        z: 100
        anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        width: Math.min(toastText.implicitWidth + 28, root.width - 40)
        height: toastText.implicitHeight + 16
        radius: Theme.radiusS
        visible: opacity > 0
        opacity: 0
        color: toast.typeOk ? Theme.noticeOkBg : Theme.noticeErrBg
        border.width: 1
        border.color: toast.typeOk ? Theme.ok : Theme.danger
        property bool typeOk: true
        function show(type, text) {
            toastText.text = text
            typeOk = (type === "ok")
            toast.opacity = 1
            toastTimer.restart()
        }
        Behavior on opacity { NumberAnimation { duration: 250 } }
        Text {
            id: toastText
            anchors.centerIn: parent
            font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
            color: toast.typeOk ? Theme.ok : Theme.danger
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }

    Column {
        anchors.fill: parent

        // ==================== 顶栏（对齐 .topbar） ====================
        Rectangle {
            width: parent.width
            height: 44   // padding 10px + 内容约 24px
            color: Theme.panelBg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

            // 左侧：标题
            Text {
                anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
                text: "🕓 Time Machine+"
                font.pixelSize: Theme.fontL; font.weight: Font.DemiBold
                font.family: Theme.fontUi; color: Theme.textPrimary
            }

            // 右侧：状态 + 重试 + 设置
            Row {
                id: topRightRow
                anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                spacing: 10
                Row {
                    spacing: 5
                    anchors.verticalCenter: parent.verticalCenter
                    Rectangle {
                        width: 7; height: 7; radius: 3.5
                        color: root.healthOk ? Theme.ok : Theme.danger
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: root.healthStatus
                        font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                        color: Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                AppButton {
                    visible: !root.healthOk
                    text: "重试"
                    variant: "tiny"
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: root.checkHealth()
                }
                AppButton {
                    text: "⚙️ 设置"
                    bold: true
                    hoverAccent: true
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: settingsDlg.open()
                }
            }

            // 居中搜索框（绝对居中，宽 min(520, 宽度余量)）
            Rectangle {
                anchors.centerIn: parent
                width: Math.min(Theme.searchMaxWidth, root.width - 420)
                height: 26
                radius: Theme.radiusPill
                color: Theme.panelBg
                border.width: 1
                border.color: searchInput.activeFocus ? Theme.accent : Theme.inputBorder
                Behavior on border.color { ColorAnimation { duration: 150 } }

                TextInput {
                    id: searchInput
                    anchors { fill: parent; leftMargin: 14; rightMargin: clearBtn.width + 14 }
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textPrimary
                    clip: true
                    selectByMouse: true
                    onTextChanged: {
                        root.searchQuery = text
                        searchDebounce.restart()
                    }
                    Keys.onEscapePressed: {
                        text = ""
                        root.searchQuery = ""
                        focus = false
                    }
                    // I-Beam 光标：裸 TextInput 不自动切换鼠标形状（TextField 才有），
                    // HoverHandler 只处理悬停、不拦截点击/选择
                    HoverHandler { cursorShape: Qt.IBeamCursor }
                    Text {
                        visible: searchInput.text.length === 0 && !searchInput.activeFocus
                        anchors.verticalCenter: parent.verticalCenter
                        text: "搜索文件名…"
                        font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                        color: Theme.textMuted
                    }
                }
                // 清除按钮 ✕
                Text {
                    id: clearBtn
                    visible: root.searchQuery.length > 0
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: "✕"
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textTertiary
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -6
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { searchInput.text = ""; root.searchQuery = "" }
                    }
                }
            }
        }

        // ==================== 主区域（grid: 320px 1fr → 窄窗口单列） ====================
        Item {
            width: parent.width
            height: parent.height - 44

            property bool narrow: width < Theme.responsiveBreakpoint

            // 左侧：时间轴面板（320px，白底右边线）
            Rectangle {
                id: timelinePanel
                anchors.left: parent.left
                anchors.top: parent.top
                width: parent.narrow ? parent.width : Theme.timelineWidth
                height: parent.narrow ? Math.min(280, parent.height * 0.4) : parent.height
                color: Theme.panelBg
                Rectangle {
                    visible: !parent.parent.narrow
                    anchors.right: parent.right
                    width: 1; height: parent.height
                    color: Theme.border
                }
                Rectangle {
                    visible: parent.parent.narrow
                    anchors.bottom: parent.bottom
                    width: parent.width; height: 1
                    color: Theme.border
                }

                TimelinePane {
                    id: timeline
                    anchors.fill: parent
                    onSelectBackup: function(b) {
                        root.currentBackup = b
                        root.highlightPath = ""
                    }
                }
            }

            // 右侧：内容面板（搜索态 / 文件列表态切换，对齐 <SearchResults v-if>）
            Item {
                id: contentPanel
                anchors.left: parent.narrow ? parent.left : timelinePanel.right
                anchors.top: parent.narrow ? timelinePanel.bottom : parent.top
                width: parent.narrow ? parent.width : parent.width - Theme.timelineWidth
                height: parent.narrow ? parent.height - timelinePanel.height : parent.height

                Rectangle {
                    anchors { fill: parent; margins: 16 }
                    radius: Theme.radiusL
                    color: Theme.panelBg
                    border.width: 1; border.color: Theme.borderSoft
                    clip: true

                    Item {
                        anchors { fill: parent; margins: 14 }

                        // 搜索态
                        SearchResultsPane {
                            id: searchPane
                            anchors.fill: parent
                            visible: root.searchQuery.trim().length > 0
                            onLocate: function(backupId, filepath) { root.handleLocate(backupId, filepath) }
                        }

                        // 文件列表态
                        BackupFileListPane {
                            id: fileListPane
                            anchors.fill: parent
                            visible: root.searchQuery.trim().length === 0
                            backup: root.currentBackup
                            highlightPath: root.highlightPath
                        }
                    }
                }
            }
        }
    }

    // ---- 设置模态（对齐 SettingsModal Teleport 到 body） ----
    SettingsDialog {
        id: settingsDlg
        onClosed: root.checkHealth()
    }
}
