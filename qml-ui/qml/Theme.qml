// Theme.qml - 全局视觉主题（1:1 对齐 electron-ui 的 CSS 变量与硬编码色值）
// 占位说明：所有颜色/字号/圆角集中在此，后续换肤只改这里
pragma Singleton
import QtQuick 2.15

QtObject {
    // ---- 背景 ----
    readonly property color bg:            "#f5f5f7"   // body/app 背景
    readonly property color panelBg:       "#ffffff"   // 面板/顶栏
    readonly property color itemBg:        "#fafafa"   // 列表项底色
    readonly property color itemHover:     "#f0f4ff"   // 列表项 hover
    readonly property color itemActive:    "#e8f0ff"   // 时间轴选中

    // ---- 边框/分隔线 ----
    readonly property color border:        "#e5e5ea"   // 顶栏下边线、面板分隔
    readonly property color borderSoft:    "#eeeeee"   // 次级分隔线
    readonly property color inputBorder:   "#dddddd"   // 输入框/按钮边框

    // ---- 文字 ----
    readonly property color textPrimary:   "#333333"
    readonly property color textSecondary: "#888888"
    readonly property color textTertiary:  "#999999"
    readonly property color textMuted:     "#bbbbbb"

    // ---- 品牌色 ----
    readonly property color accent:        "#007aff"   // 主按钮/强调
    readonly property color accentDark:    "#0066d6"   // 主按钮 hover
    readonly property color accentSoft:    "#e8f0ff"   // 版本徽章底色
    readonly property color warning:       "#ff9500"   // 进行中
    readonly property color ok:            "#22aa77"   // 成功
    readonly property color danger:        "#cc0000"   // 失败/删除
    readonly property color highlightBg:   "#fff7e0"   // 搜索定位高亮底
    readonly property color highlightBorder: "#ffd60a" // 搜索定位高亮边/mark
    readonly property color noticeOkBg:    "#e3f7ea"
    readonly property color noticeErrBg:   "#ffe3e3"
    readonly property color dangerBorder:  "#ffbbbb"
    readonly property color dangerBg:      "#ffeeee"
    readonly property color errInput:      "#ee3333"

    // ---- 圆角 ----
    readonly property int radiusS:  6    // 按钮/输入框
    readonly property int radiusM:  8    // 列表项/时间轴节点
    readonly property int radiusL:  10   // 面板
    readonly property int radiusXL: 12   // 模态窗口
    readonly property int radiusPill: 16 // 搜索框

    // ---- 字号（对齐 CSS px） ----
    readonly property int fontXS: 10
    readonly property int fontS:  11
    readonly property int fontM:  12
    readonly property int fontBase: 13
    readonly property int fontL:  14
    readonly property int fontXL: 15

    // ---- 字体族 ----
    // 逗号分隔的回退栈（Qt 5.13+ 支持，逐字符匹配）：
    // 西文优先 Segoe UI（对齐 Electron 版），中文字形 Segoe UI 不含 → 回退微软雅黑。
    // 注意：不写回退栈时 Qt5 系统回退常命中宋体（衬线细笔画），必须显式列出。
    // 跨平台降级：macOS PingFang SC → Linux Noto Sans CJK SC → 系统 sans-serif。
    readonly property string fontUi:   "Segoe UI, Microsoft YaHei UI, Microsoft YaHei, PingFang SC, Noto Sans CJK SC, sans-serif"
    // 等宽栈用于路径显示；路径可能含中文，同样需要雅黑回退
    readonly property string fontMono: "Consolas, Microsoft YaHei UI, Microsoft YaHei, monospace"

    // ---- 布局常量 ----
    readonly property int topBarHeight:   44   // 10px padding + 内容
    readonly property int timelineWidth:  320  // grid-template-columns: 320px 1fr
    readonly property int responsiveBreakpoint: 860  // @media (max-width: 860px)
    readonly property int searchMaxWidth: 520
    readonly property int modalWidth:     640
}
