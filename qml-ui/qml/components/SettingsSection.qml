// SettingsSection.qml - 设置页通用节：标题 + 描述 + 输入行 + 列表
// 源路径/目标路径两节共用（结构完全一致，抽取消除重复）
import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Column {
    id: root

    property string icon: ""
    property string title: ""
    property string description: ""
    property string placeholder: ""
    property string inputText: ""
    property string errorText: ""
    property var listModel: []
    property bool isTarget: false        // 目标路径显示剩余空间
    property var humanSizeFunc: null

    signal inputChanged(string text)
    signal submit()
    signal remove(int id)

    spacing: 10
    leftPadding: 20; rightPadding: 20; topPadding: 16; bottomPadding: 16

    // 标题行
    Row {
        spacing: 8
        Text {
            text: root.icon
            font.pixelSize: Theme.fontL; font.family: Theme.fontUi
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.title
            font.pixelSize: Theme.fontL; font.weight: Font.DemiBold
            font.family: Theme.fontUi; color: Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Text {
        text: root.description
        font.pixelSize: Theme.fontM; font.family: Theme.fontUi
        color: Theme.textSecondary
        width: parent.width - 40
        wrapMode: Text.WordWrap
    }

    // 输入行：输入框 + 添加按钮
    Row {
        width: parent.width - 40
        spacing: 10
        Rectangle {
            width: parent.width - addBtn.width - 10
            height: 32
            radius: Theme.radiusS
            color: Theme.panelBg
            border.width: 1
            border.color: root.errorText.length > 0 ? Theme.errInput
                          : (input.activeFocus ? Theme.accent : Theme.inputBorder)
            Behavior on border.color { ColorAnimation { duration: 150 } }

            TextInput {
                id: input
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                verticalAlignment: TextInput.AlignVCenter
                text: root.inputText
                font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                color: Theme.textPrimary
                clip: true
                selectByMouse: true
                onTextChanged: root.inputChanged(text)
                Keys.onReturnPressed: root.submit()
                Keys.onEnterPressed: root.submit()
                HoverHandler { cursorShape: Qt.IBeamCursor }

                // 占位符
                Text {
                    visible: input.text.length === 0 && !input.activeFocus
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.placeholder
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
                    color: Theme.textMuted
                }
            }
        }
        AppButton {
            id: addBtn
            text: "添加"
            variant: "primary"
            onClicked: root.submit()
        }
    }

    // 错误提示
    Text {
        visible: root.errorText.length > 0
        text: root.errorText
        font.pixelSize: Theme.fontM; font.family: Theme.fontUi
        color: Theme.errInput
    }

    // 列表
    Column {
        width: parent.width - 40
        spacing: 8

        Repeater {
            model: root.listModel
            delegate: Rectangle {
                width: parent.width
                height: 44
                radius: Theme.radiusM
                color: Theme.itemBg
                border.width: 1; border.color: Theme.border

                Text {
                    anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                    width: parent.width - 24 - (root.isTarget ? freeText.width + 8 : 0) - delBtn.width - 12
                    text: modelData.path
                    font.pixelSize: Theme.fontBase; font.family: Theme.fontMono
                    color: Theme.textPrimary
                    elide: Text.ElideMiddle
                }

                Text {
                    id: freeText
                    visible: root.isTarget
                    anchors { right: delBtn.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: root.humanSizeFunc ? ("剩余 " + root.humanSizeFunc(modelData.spaceRemain)) : ""
                    font.pixelSize: Theme.fontM; font.family: Theme.fontUi
                    color: Theme.textSecondary
                }

                AppButton {
                    id: delBtn
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: "删除"
                    variant: "danger"
                    onClicked: root.remove(modelData.id)
                }
            }
        }

        // 空态
        Text {
            visible: root.listModel.length === 0
            text: "尚未添加任何路径"
            font.pixelSize: Theme.fontBase; font.family: Theme.fontUi
            color: Theme.textTertiary
            topPadding: 4; bottomPadding: 4
        }
    }
}
