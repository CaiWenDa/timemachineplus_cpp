# QML 布局教程：面向 CSS 开发者

> 目标读者：熟悉 CSS（Flexbox/Grid/定位），刚接触 QML 的开发者
> 学习时长：1-2 小时
> Qt 版本：Qt 6（兼容 Qt 5.15+）
> 核心理念：**不重复造概念，用你已有的 CSS 知识做锚点，聚焦"QML 有什么不同"**

---

## 0. 先建立心智模型：CSS vs QML 的本质差异

CSS 和 QML 看起来都像"声明式界面"，但底层模型完全不同：

| 维度 | CSS + HTML | QML |
|------|-----------|-----|
| 布局默认行为 | **文档流**（block 从上到下、inline 从左到右） | **无流**：所有 Item 默认在 (0,0) 重叠，必须显式声明位置 |
| 子元素与父容器 | 子元素自动在父容器内流动、换行 | 子元素**不会自动排列**，必须用 `Row`/`Column`/`anchors` 等显式驱动 |
| 尺寸来源 | 内容撑开 / 百分比 / 显式值 | 必须显式给出 `width/height`，或由布局容器分配 |
| 状态与样式 | CSS 无状态，靠 JS 切 class | 每个属性都是可绑定表达式，`width: a > b ? 100 : 200` 是常态 |
| 单位 | px / rem / % / vw | 像素（无单位）/ 百分比（`parent.width * 0.5`） |

> **一句话记忆**：CSS 是"元素默认会自己待好，你写样式微调"；QML 是"元素默认重叠在原点，你必须告诉它去哪、多大"。

---

## 1. 概念映射表：CSS 布局模式 → QML 组件

| CSS 布局模式 | QML 等价组件 | 相同点 | 核心差异 |
|-------------|-------------|--------|---------|
| `display: flex` (row) | `RowLayout` / `Row` | 子元素横向排列 | QML 需显式指定间距 `spacing`，无 `gap` 属性 |
| `display: flex` (column) | `ColumnLayout` / `Column` | 子元素纵向排列 | 同上 |
| `display: grid` | `GridLayout` / `Grid` | 二维网格排列 | QML 的 Grid 是自动流填充，GridLayout 支持行列跨度 |
| 标签页 / 堆叠面板 | `StackLayout` / `StackView` | 同一位置只显示一个 | QML 用 `currentIndex` 切换，无 CSS 对应 |
| `position: absolute` 边对齐 | `anchors` 锚定系统 | 靠父/兄弟边缘定位 | QML 是声明式约束，非"脱离文档流" |
| `margin` | `anchors.margins` / `Layout.margins` | 外边距 | 语义略不同，见第 3 节 |
| `padding` | 容器内嵌套子 Item + `anchors.margins` | 内边距 | `Item`/`Rectangle` 无原生 padding，需嵌套实现 |
| `position: fixed` / `sticky` | `ApplicationWindow` 顶层 Item + z | 脱离流固定 | QML 用 `z` 层级和顶层位置实现 |

### 两套布局体系的选择

QML 有**两套独立的布局体系**，不要混用在同一个 Item 上：

| 体系 | 组件 | 适用场景 |
|------|------|---------|
| **Layouts**（流式） | `RowLayout`、`ColumnLayout`、`GridLayout`、`StackLayout` | 子元素数量/尺寸动态变化，需要自动分配空间 |
| **anchors**（锚定） | 任何 Item 的 `anchors` 属性 | 固定比例、贴边、居中、相对兄弟定位 |

> **铁律**：同一个 Item 不要同时用 `anchors` 和 `Layout.*` 决定尺寸/位置，否则行为未定义。外层用 anchors 定区块，内层用 Layout 排内容。

---

## 2. 核心布局组件详解（含 CSS 对比）

所有示例均可直接运行。通用前置：

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
```

### 2.1 ColumnLayout — 纵向流式排列 ≈ `flex-direction: column`

**CSS 实现：**
```css
.container { display: flex; flex-direction: column; gap: 8px; padding: 16px; }
.item { flex: 1; }
```

**QML 等价实现：**
```qml
ColumnLayout {
    anchors.fill: parent
    anchors.margins: 16       // padding: 16px
    spacing: 8                // gap: 8px

    Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; color: "red" }
    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 60; color: "green" }
    Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; color: "blue" }
}
```

**关键差异**：
- CSS 的 `flex: 1` → QML 的 `Layout.fillHeight: true`
- CSS 子元素默认宽度填满容器 → QML 必须显式 `Layout.fillWidth: true`，否则宽度为 0

### 2.2 RowLayout — 横向流式排列 ≈ `flex-direction: row`

**CSS 实现：**
```css
.container { display: flex; flex-direction: row; gap: 8px; height: 100px; }
.item { flex: 1; }
```

**QML 等价实现：**
```qml
RowLayout {
    anchors.fill: parent
    spacing: 8

    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "red" }
    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "green" }
    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "blue" }
}
```

### 2.3 GridLayout — 网格 ≈ `display: grid`

**CSS 实现：**
```css
.container { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
```

**QML 等价实现：**
```qml
GridLayout {
    anchors.fill: parent
    columns: 3
    columnSpacing: 8
    rowSpacing: 8

    Repeater {
        model: 9
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 80
            color: ["red","green","blue","yellow","purple","orange","cyan","pink","gray"][index]
        }
    }
}
```

**跨列跨行**（对应 `grid-column: span 2`）：
```qml
Rectangle {
    Layout.columnSpan: 2
    Layout.rowSpan: 1
    Layout.fillWidth: true
    color: "gold"
}
```

### 2.4 StackLayout — 堆叠切换 ≈ 标签页

CSS 无直接对应（需 JS 切换 `display`）。

**QML 实现：**
```qml
StackLayout {
    id: stack
    anchors.fill: parent
    currentIndex: 1   // 显示第二个子项

    Rectangle { color: "red"; Text { text: "页面 1"; anchors.centerIn: parent } }
    Rectangle { color: "green"; Text { text: "页面 2"; anchors.centerIn: parent } }
    Rectangle { color: "blue"; Text { text: "页面 3"; anchors.centerIn: parent } }
}
// 切换：stack.currentIndex = 2
```

---

## 3. QML 独有：anchors 锚定系统

anchors 是 QML 最核心、最常用的布局机制。CSS 开发者可以把它理解为**声明式的 `position: absolute` + `margin`**，但更强大。

### 3.1 七条锚线

每个 Item 有 7 个可锚定的属性：`left`、`right`、`top`、`bottom`、`horizontalCenter`、`verticalCenter`、`baseline`。

| anchors 写法 | CSS 等价 |
|-------------|---------|
| `anchors.left: parent.left` | `position: absolute; left: 0` |
| `anchors.fill: parent` | `position: absolute; inset: 0` |
| `anchors.centerIn: parent` | `position: absolute; top:50%; left:50%; transform: translate(-50%,-50%)` |
| `anchors.right: sibling.left` | 兄弟元素右贴左（CSS 需 flex 或绝对定位计算） |
| `anchors.horizontalCenter: parent.horizontalCenter` | `left: 50%; transform: translateX(-50%)` |

### 3.2 margin（外边距）

```qml
Item {
    anchors.fill: parent
    anchors.margins: 16        // 四边 16px
    anchors.leftMargin: 20     // 单独覆盖左边
    anchors.topMargin: 12
}
```

**与 CSS margin 的差异**：
- CSS `margin` 影响元素在文档流中的占位
- QML `anchors.margins` 是锚定线的**偏移量**，不改变元素自身尺寸，只改变位置

### 3.3 padding（内边距）的实现

`Item` 和 `Rectangle` **没有原生 padding**。标准做法是嵌套一层子 Item：

```qml
Rectangle {
    width: 300; height: 200
    color: "white"
    border.color: "gray"

    // 这层 Item 实现 padding: 16
    Item {
        anchors.fill: parent
        anchors.margins: 16
        // 真正的内容放这里
        Text { text: "带 padding 的内容"; anchors.centerIn: parent }
    }
}
```

对应 CSS：
```css
.box { padding: 16px; border: 1px solid gray; }
```

### 3.4 anchors 的优势：声明式约束

CSS 绝对定位需要手动计算坐标，anchors 是**约束**——你声明关系，引擎自动计算且实时更新：

```qml
Rectangle {
    id: sidebar
    width: 240; anchors.fill: parent
}

Rectangle {
    // 左边永远贴着 sidebar 的右边，sidebar 宽度变化时自动跟随
    anchors.left: sidebar.right
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.right: parent.right
}
```

CSS 要实现同样效果需要 JS 监听 sidebar 宽度变化并重算。

---

## 4. Layout 附加属性详解

在 `RowLayout`/`ColumnLayout`/`GridLayout` 内部的子元素，可以用 `Layout.*` 附加属性控制尺寸分配。这些属性**只在 Layout 容器内有效**。

| 属性 | 作用 | CSS 对应 |
|------|------|---------|
| `Layout.fillWidth: true` | 宽度填满可用空间 | `flex: 1` (主轴) |
| `Layout.fillHeight: true` | 高度填满可用空间 | `flex: 1` (主轴) |
| `Layout.preferredWidth` | 首选宽度（未拉伸时的基准） | `flex-basis` |
| `Layout.preferredHeight` | 首选高度 | `flex-basis` |
| `Layout.minimumWidth` | 最小宽度（压缩下限） | `min-width` |
| `Layout.minimumHeight` | 最小高度 | `min-height` |
| `Layout.maximumWidth` | 最大宽度（拉伸上限） | `max-width` |
| `Layout.maximumHeight` | 最大高度 | `max-height` |
| `Layout.alignment` | 在分配空间内的对齐方式 | `align-self` / `justify-self` |

### 使用场景示例

```qml
ColumnLayout {
    anchors.fill: parent
    spacing: 8

    // 固定高度的头部（不参与拉伸）
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 50
        color: "steelblue"
    }

    // 可伸缩的内容区，吸收剩余空间
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 100    // 窗口再小也至少 100px
        color: "lightgray"
    }

    // 固定高度的底部
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        color: "darkgray"
    }
}
```

> **对应 CSS Flexbox**：头部 `flex: 0 0 50px`，内容 `flex: 1 1 auto; min-height: 100px`，底部 `flex: 0 0 30px`。

---

## 5. 响应式布局

### 5.1 CSS 媒体查询 vs QML 绑定式响应

CSS 用媒体查询断点切换样式：
```css
.sidebar { width: 240px; }
@media (max-width: 860px) {
    .sidebar { width: 100%; }
    .main { margin-left: 0; }
}
```

QML 用**属性绑定**直接根据窗口尺寸计算（无断点声明，实时响应）：
```qml
property bool narrow: root.width < 860

Rectangle {
    width: narrow ? root.width : 240
    // 窄窗口占满，宽窗口固定 240
}
```

### 5.2 实战：固定侧边栏 + 自适应主内容区

本项目 [Main.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml) 就是这个模式的典范。

**CSS 实现：**
```css
.app { display: flex; height: 100vh; }
.sidebar { width: 320px; flex-shrink: 0; }
.content { flex: 1; }
@media (max-width: 860px) {
    .app { flex-direction: column; }
    .sidebar { width: 100%; height: 40%; }
}
```

**QML 等价实现：**
```qml
Item {
    id: root
    anchors.fill: parent
    property bool narrow: width < 860

    // 侧边栏
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        width: narrow ? parent.width : 320
        height: narrow ? Math.min(280, parent.height * 0.4) : parent.height
        color: "#f5f5f5"
    }

    // 主内容区
    Rectangle {
        anchors.left: narrow ? parent.left : sidebar.right
        anchors.top: narrow ? sidebar.bottom : parent.top
        width: narrow ? parent.width : parent.width - 320
        height: narrow ? parent.height - sidebar.height : parent.height
        color: "white"
    }
}
```

**核心差异**：CSS 媒体查询是"断点触发样式切换"，QML 绑定是"每个属性独立根据条件计算"。QML 更灵活但需要手动处理每个相关属性。

---

## 6. 常见布局场景：CSS vs QML 对照

### 场景 1：居中布局

**CSS：**
```css
.parent { display: flex; justify-content: center; align-items: center; }
```

**QML：**
```qml
Item {
    anchors.fill: parent
    Rectangle {
        anchors.centerIn: parent   // 水平垂直居中
        width: 100; height: 100
        color: "red"
    }
}
```

### 场景 2：等高列

**CSS：**
```css
.row { display: flex; }
.col { flex: 1; }
```

**QML：**
```qml
RowLayout {
    anchors.fill: parent
    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "red" }
    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "green" }
    Rectangle { Layout.fillHeight: true; Layout.fillWidth: true; color: "blue" }
}
```

### 场景 3：等分布局（按比例）

**CSS：**
```css
.row { display: flex; }
.col-1 { flex: 1; }
.col-2 { flex: 2; }
```

**QML（用 Layout.fillWidth + preferredWidth 比例近似）：**
```qml
RowLayout {
    anchors.fill: parent
    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.preferredWidth: 1   // 权重 1
        color: "red"
    }
    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.preferredWidth: 2   // 权重 2
        color: "green"
    }
}
```

### 场景 4：固定侧边栏 + 自适应主内容

见第 5.2 节。

---

## 7. 常见认知陷阱（CSS 习惯迁移错误）

### 陷阱 1：以为子元素会自动排列
**错误**：在 `Rectangle` 里放两个子元素，以为会像 div 一样从上到下排。
**事实**：两个子元素会**重叠在 (0,0)**。必须用 `Column`/`Row`/`ColumnLayout` 包裹，或给子元素设 anchors/y。

### 陷阱 2：给 Item 写不存在的 padding/margin 属性
**错误**：`Rectangle { padding: 16 }` 或 `Text { marginTop: 8 }`。
**事实**：`Item`/`Rectangle`/`Text` 都没有这些属性，运行时报"不存在的属性"。
**正确**：嵌套子 Item 用 `anchors.margins` 实现 padding；用 `spacing` 或额外 Item 占位实现 margin。

### 陷阱 3：同一 Item 混用 anchors 和 Layout
**错误**：`Item { anchors.fill: parent; Layout.fillWidth: true }`。
**事实**：两套体系冲突，行为未定义，常导致尺寸为 0 或位置错乱。
**正确**：外层容器用 anchors 定位区块，内层用 Layout 排内容；同一个 Item 只选一套。

### 陷阱 4：手写减法高度
**错误**：`height: parent.height - 44 - 16`（减去顶栏和边距）。
**事实**：常量与真实 spacing/margins 不同步时，不同 DPI/字体下会负高度或重叠。
**正确**：用 `ColumnLayout` + `Layout.fillHeight: true`，让引擎自动分配剩余空间。

### 陷阱 5：忘记设 width/height
**错误**：`Rectangle { color: "red" }` 不加尺寸。
**事实**：默认 `width: 0; height: 0`，看不见。
**正确**：要么设显式尺寸，要么 `anchors.fill` / `Layout.fillWidth`。

### 陷阱 6：以为 anchors.margins = padding
**错误**：给容器设 `anchors.margins: 16` 以为内容会内缩。
**事实**：`anchors.margins` 是容器相对锚定目标的偏移（外边距），不影响子元素。
**正确**：padding 要嵌套子 Item：`Item { anchors.fill: parent; anchors.margins: 16 /* 内容区 */ }`。

### 陷阱 7：用 Grid 期望像 CSS Grid 一样控制行列
**错误**：`Grid { columns: 3 }` 然后期望手动指定每个元素的行列位置。
**事实**：`Grid` 是自动流填充，按子元素顺序依次填入。需要手动定位用 `GridLayout` + `Layout.row`/`Layout.column`。

---

## 8. 布局决策流程图

```
开始布局一个容器
│
├─ 子元素数量固定、尺寸已知、位置关系明确？
│   ├─ 是 → 用 anchors 锚定（贴边、居中、相对兄弟）
│   └─ 否 → 进入 Layout 体系
│
├─ Layout 体系内：
│   ├─ 子元素横向排列？ → RowLayout
│   ├─ 子元素纵向排列？ → ColumnLayout
│   ├─ 二维网格？ → GridLayout
│   └─ 堆叠切换显示一个？ → StackLayout
│
├─ 需要滚动？ → 外层包 ScrollView，内层用 Layout
│
└─ 注意事项：
    ├─ 同一 Item 不混用 anchors + Layout
    ├─ 子元素必须设尺寸或 fillWidth/fillHeight
    ├─ 用 preferredHeight/minimumHeight 代替硬编码减法
    └─ padding 用嵌套 Item + anchors.margins 实现
```

### 快速决策口诀

| 你想做的事 | 选什么 |
|-----------|--------|
| 子元素一个接一个横排 | `RowLayout` |
| 子元素一个接一个竖排 | `ColumnLayout` |
| 整齐的网格 | `GridLayout` |
| 贴父容器某条边 | `anchors.left/right/top/bottom: parent.xxx` |
| 居中 | `anchors.centerIn: parent` |
| 填满父容器 | `anchors.fill: parent` |
| 两个元素并排贴在一起 | `anchors.left: sibling.right` |
| 某个区域占满剩余空间 | `Layout.fillWidth/Height: true` |
| 标签页切换 | `StackLayout` + `currentIndex` |

---

## 9. 一份完整可运行的示例

把下面代码存为 `Demo.qml`，用 `qmlscene Demo.qml` 或 Qt Creator 运行：

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    visible: true
    width: 800
    height: 500
    title: "QML Layout Demo"

    // 顶栏 + 主体 两栏布局
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶栏（固定高度）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#2c3e50"
            Text {
                anchors.centerIn: parent
                text: "顶部导航栏"
                color: "white"
                font.pixelSize: 16
            }
        }

        // 主体：左侧边栏 + 右侧内容
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 侧边栏（固定宽度）
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 200
                color: "#ecf0f1"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8
                    Repeater {
                        model: ["首页", "文件", "设置"]
                        Text { text: modelData; font.pixelSize: 14 }
                    }
                }
            }

            // 右侧内容（自适应宽度）
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "white"
                Text {
                    anchors.centerIn: parent
                    text: "主内容区（自适应宽度）"
                    font.pixelSize: 20
                    color: "#333"
                }
            }
        }
    }
}
```

---

## 总结：1 小时上手路径

1. **10 分钟**：理解"QML 无文档流，必须显式定位"（第 0 节）
2. **15 分钟**：掌握 `anchors` 七条线 + `fill`/`centerIn`/`margins`（第 3 节）
3. **15 分钟**：掌握 `ColumnLayout`/`RowLayout` + `fillWidth`/`fillHeight`/`preferredHeight`（第 2、4 节）
4. **10 分钟**：理解响应式用属性绑定而非媒体查询（第 5 节）
5. **10 分钟**：记住 7 个陷阱，避免 CSS 习惯错误迁移（第 7 节）
6. **剩余时间**：跑通第 9 节完整示例，对照自己项目的 [Main.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml) 理解真实用法

掌握以上内容后，常规界面布局即可独立完成。进阶再学习 `GridLayout` 跨列、`LayoutMirroring`（RTL 镜像）、`Flickable`/`ScrollView` 滚动策略等。
