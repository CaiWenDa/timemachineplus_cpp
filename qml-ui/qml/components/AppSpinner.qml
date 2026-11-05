// AppSpinner.qml - 对齐 .spinner：14px 圆环旋转加载指示器（.8s 线性无限）
import QtQuick 2.15
import ".."

Item {
    id: root
    property int size: 14
    property real borderWidth: 2
    property color ringColor: Theme.inputBorder
    property color headColor: Theme.accent

    implicitWidth: size
    implicitHeight: size

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.headColor
            ctx.lineWidth = root.borderWidth
            ctx.lineCap = "round"
            var r = (width - root.borderWidth) / 2
            ctx.beginPath()
            // 画 270 度弧，模拟 border-top-color 的缺口圆环
            ctx.arc(width / 2, height / 2, r, 0, Math.PI * 1.5)
            ctx.stroke()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        RotationAnimator on rotation {
            from: 0; to: 360
            duration: 800
            loops: Animation.Infinite
            running: root.visible
        }
    }
}
