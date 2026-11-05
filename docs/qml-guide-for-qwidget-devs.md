# QML 快速入门指南 —— 面向 QWidget 开发者

> 目标：能读懂本项目 `qml-ui/` 目录下的全部 QML 代码。只讲"读代码需要的最小知识"，跳过与项目无关的内容。

---

## 1. 核心范式差异（最重要的一张表）

| | QWidget | QML |
|---|---|---|
| 编程范式 | **命令式**：C++ 代码逐步构造对象 | **声明式**：描述"界面长什么样"，引擎负责创建 |
| 对象关系 | 父子树（`parent` 指针） | 对象树（缩进嵌套即父子） |
| 布局 | `QLayout`（QHBoxLayout/QVBoxLayout） | 定位器 `Row/Column` + **锚点 `anchors`** |
| 数据流 | `setText()` 一次性赋值 | **属性绑定**：`text: modelData.day` 是持续生效的表达式 |
| 信号连接 | `connect(btn, &QPushButton::clicked, ...)` | `onClicked: { ... }` 或 `Connections {}` |
| 样式 | `setStyleSheet`（CSS 子集） | 直接写属性：`color`、`radius`、`border.width` |
| 渲染 | 软件光栅化（QPaintEngine） | **Scene Graph（GPU）**，绘制的是场景图节点 |
| 动画 | `QPropertyAnimation` 手动启动 | `Behavior on x {}` 声明即生效 |

**关键思维转换**：QML 属性赋值语句不是"执行一次的赋值"，而是"建立一个等式"。读到 `width: parent.width - 40` 时，要理解成"无论何时 parent 宽度变化，这里都会自动重算"——这等价于 QWidget 里每次 `resizeEvent` 都手动调一次 `setFixedWidth()`。

---

## 2. 最小语法集

以 [TimelinePane.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml) 为例覆盖 90% 场景：

```qml
Item {                              // ① 对象声明：类型名 + {}（≈ new Item）
    id: root                        // ② id：对象在文件内的唯一引用名（≈ 成员变量名）
    property var backups: []        // ③ 自定义属性（≈ 成员变量，自带 changed 信号）
    signal selectBackup(var backup) // ④ 自定义信号（≈ signals: 声明）
    function reload() { ... }       // ⑤ 成员函数，写 JS

    onBackupsChanged: { ... }       // ⑥ 内置信号处理器：属性变化时触发

    Text { text: "..." }            // ⑦ 嵌套子对象，父对象是其 parent
}
```

要点：

- **`on<PropertyName>Changed`**：任何属性都有隐式的 changed 信号，`onXxxChanged` 就是它的槽。`onClicked`、`onTriggered` 同理对应信号名。
- **表达式里可直接用其他对象的 id 和属性**：`visible: root.backups.length === 0`，引擎自动追踪依赖，任意被引用的属性变化都会重新求值。
- `property var` 是无类型（可存任何 JS 值），还有 `int/string/real/bool/color/list` 等强类型。
- 注释 `//` 和 `/* */` 与 C++ 相同。

---

## 3. 常用组件 × QWidget 对照（本项目实际用到的）

| QML 组件 | QWidget 对应 | 本项目实例 |
|---|---|---|
| `ApplicationWindow` | `QMainWindow` | [Main.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml) |
| `Item` | 无背景的 `QWidget`（纯容器） | 各面板容器 |
| `Rectangle` | 带样式的 `QFrame`/`QWidget`（color/radius/border） | 顶栏、卡片、分隔线 |
| `Text` | `QLabel` | 到处 |
| `TextInput` | `QLineEdit` | [Main.qml 搜索框](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml#L173) |
| `Row` / `Column` | `QHBoxLayout` / `QVBoxLayout` | 注意：是**定位器**不是布局管理器 |
| `Flickable` + `ScrollBar` | `QScrollArea` | [TimelinePane.qml#L131](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml#L131) |
| `Repeater` | `for` 循环 `new QWidget()` 一批 | [TimelinePane.qml#L168](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml#L168) |
| `MouseArea` | 重写 `mousePressEvent` 等 / `eventFilter` | [TimelinePane.qml#L270](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml#L270) |
| `Timer` | `QTimer` | [Main.qml#L50](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml#L50)（搜索防抖） |
| `Popup`（modal） | 模态 `QDialog` | [ConfirmDialog.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/ConfirmDialog.qml)、[SettingsDialog.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/SettingsDialog.qml) |
| `Behavior` | `QPropertyAnimation`（自动版） | [TimelinePane.qml#L212](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml#L212) |
| `Connections` | `connect()` 语句 | 见下节 |

---

## 4. 信号与槽的 QML 写法

QWidget 里一个 `connect` 对应 QML 里的**三种**写法：

```qml
// 写法 A：属性式处理器（最常用），≈ connect(obj, sig, this, slot)
AppButton { onClicked: root.reload() }        // AppButton 内部 signal clicked()

// 写法 B：Connections 块（监听**外部**对象，≈ connect(otherObj, sig, this, slot)）
// Main.qml#L32 —— 监听 C++ backend 对象的信号：
Connections {
    target: backend                            // connect 的第一个参数
    function onHealthReady(ok, text) { ... }   // 槽：on + 信号名（首字母大写）
}

// 写法 C：声明自己的信号供别人连
signal selectBackup(var backup)   // TimelinePane.qml#L12
selectBackup(modelData)           // 发射 = 当函数调用
// 外部：TimelinePane { onSelectBackup: function(b) { ... } }
```

规则：**信号 `fooBar` 的处理器固定叫 `onFooBar`**。读代码时看到 `on` 开头的驼峰名，先判断是信号处理器还是属性绑定——处理器后面跟 `{}` 代码块。

---

## 5. QML × C++ 混合编程（本项目模式）

本项目是**同进程架构**：C++ 后端对象直接暴露给 QML，无网络/IPC。

### 模式一：上下文属性注入（本项目唯一入口）

[main.cpp#L39](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/src/main.cpp#L39)：

```cpp
BackendBridge backend;                                  // 普通 QObject 子类
engine.rootContext()->setContextProperty("backend", &backend);
engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
```

QML 侧任何文件里直接写 `backend.refreshBackups()` 即可调用 `Q_INVOKABLE` 方法；`Connections { target: backend; function onBackupsReady(...) }` 收信号。**QWidget 对照**：等价于把一个 `QObject*` 全局单例传给所有 widget，但靠名字 `"backend"` 而不是指针传递。

### 模式二：Worker 线程桥（BackendBridge）

[BackendBridge.h](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/src/BackendBridge.h)：`BackendBridge` 住主线程（QML 调它不阻塞 UI），内部 `BackendWorker` 住 `QThread` 持有 `ServiceRun`，所有 `Q_INVOKABLE` 用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection, Q_ARG(...))` 投递到 worker 线程，结果用信号回传。**这是标准 Qt 多线程模式，和 QWidget 项目里一模一样**——区别只是消费者从 widget 换成 QML。

### 模式三：QML 单例（Theme）

[Theme.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Theme.qml) 通过 `qml/qmldir` 声明 `singleton Theme 1.0 Theme.qml`，全局只构造一次，任意文件写 `Theme.accent` 引用。**QWidget 对照**：等价于 `QPalette`/样式常量单例类。

### QWidget 项目嵌入 QML（本项目未用，了解即可）

```cpp
auto *qw = new QQuickWidget(parent);
qw->setSource(QUrl("qrc:/qml/Main.qml"));   // 一个 QWidget 里渲染 QML
```
`QQuickWidget` 把整个 QML 场景当作一个 widget 塞进现有布局，适合渐进迁移。代价：渲染走离屏 FBO，有性能损耗。

---

## 6. 代码阅读定位技巧

**步骤 1：从入口找对象树骨架。** 读 [main.cpp#L40](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/src/main.cpp#L40) 找到根 QML（`qrc:/qml/Main.qml`），再顺着缩进看嵌套——**缩进层级就是父子层级**。[Main.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml) 的对象树 ≈ `ApplicationWindow → Column → [顶栏 Rectangle | 主区域 Item → [TimelinePane | SearchResultsPane / BackupFileListPane]]`。

**步骤 2：把 id 当成员变量索引。** `id: searchDebounce`、`id: toast`、`id: settingsDlg` —— 全项目里对这些 id 的引用就是"成员访问"。想理解 `searchDebounce.restart()` 先看它的声明块。

**步骤 3：识别三类代码行。**

| 形态 | 性质 | 例 |
|---|---|---|
| `prop: 值`（无 `{}`） | 属性绑定，持续生效 | `width: parent.width - 40` |
| `prop: { ... }` | 一次性 JS 求值后绑定/赋值 | `visible: { compute() }` |
| `onXxx: { ... }` | 信号处理器，事件触发 | `onClicked: { ... }` |

**步骤 4：追踪数据流而不是调用链。** QML 没有"谁调了 setText"这种问题——看到界面文字变化，反向搜属性名：`text: xxx` 的 `xxx` 从哪来。例如文件列表为空，搜 `backupFilesReady` → 找到 [Main.qml 的 Connections](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml#L32) → 再查 C++ 侧发射处。

**步骤 5：显隐逻辑看 `visible` 绑定。** 搜索态/列表态切换：`visible: root.searchQuery.length > 0` 与 `visible: root.searchQuery.length === 0` 两个互斥绑定即 Electron 版的 `v-if`。

---

## 7. 本项目高频模式速查

| 模式 | 代码位置 | QWidget 对照 |
|---|---|---|
| JS 数组 + Repeater + `modelData` | [TimelinePane.qml#L168-L174](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml#L168-L174) | `for` 循环创建子 widget，`modelData` = 当前数据元素 |
| 属性变化处理器驱动刷新 | TimelinePane `onBackupsChanged` | 观察者模式回调 |
| Behavior 隐式动画 | `Behavior on color { ColorAnimation {...} }` | 不用写动画代码，属性一变自动插值 |
| Timer 防抖 | [Main.qml#L50](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml#L50) `searchDebounce.restart()` | `QTimer::setSingleShot` + restart |
| Popup 模态 + 自定义 closed 信号 | [SettingsDialog.qml#L19](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/SettingsDialog.qml#L19) | `QDialog::exec()` + `finished` |
| 组件对外信号、对内属性（封装边界） | [SettingsSection.qml#L20-L22](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/SettingsSection.qml#L20-L22) | 自定义 widget 的 signals/public 成员 |
| 全局状态挂根窗口 + id 共享 | Main.qml 的 `searchQuery/currentBackup` 属性 | 主窗口类成员变量 |

---

## 8. 常见陷阱（与 QWidget 直觉不符处）

1. **绑定被赋值即永久失效。** `width: parent.width` 建立绑定后，若某处执行 `width = 100`，绑定被**销毁**，之后 parent 变化不再跟随。QWidget 里 `setFixedWidth` 没有副作用，这里相反。读代码时看到 JS 赋值属性，要警惕它破坏了哪个绑定。
2. **`border.width` 默认是 1，不是 0。** 无 `radius` 的 `Rectangle` 只设 `border.color` 会画出 1px 直角边框（本项目实际踩过的 bug：确认弹窗顶部出现直角细线）。要边框必须显式 `border.width`，不要边框必须不写 `border.color`。
3. **`Column` 里 `visible: false` 的子项不占空间**（定位器自动跳过）。QWidget 的 `hide()` 在 layout 里也是这行为，但**绝对定位的兄弟不会**——注意区分。
4. **`ListModel`/`ListElement` 不支持嵌套数组。** 需要 `{day, items:[...]}` 这种结构时只能用 JS 数组 + `modelData`，否则 delegate 取到的内层字段全是 `undefined`（本项目踩过）。
5. **z 顺序 = 声明顺序，后声明的压在上面。** 没有显式 `z` 时，写在后面的元素盖住前面的。本项目 [BackupFileListPane.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/BackupFileListPane.qml) 中整行 `MouseArea` 必须声明在恢复按钮**之前**，否则按钮点不到。
6. **`anchors` 只能用于父子或兄弟之间**，不能锚到任意远亲；且 `anchors` 与显式 `x/y/width/height` 混写同一方向会冲突。
7. **`Item` 完全透明且无背景**，想看到它必须加 `Rectangle`；QWidget 至少能 paintEvent。
8. **`width/height` 默认 0**（Item 无 sizeHint 概念），忘记设置就是不可见。反之 `implicitWidth` 是"内容自然尺寸"，未显式设 `width` 时生效。
9. **`var` 属性类型擦除**：存了数组后重新赋 `[]` 才触发 `changed`；原地 `push()` 不触发（引用没变），需要 `arr = arr.slice()` 或整体重赋值。本项目 `groupedData = groupByDay(backups)` 用整体重赋值规避。
10. **WIN32 子系统下 `console.log` 无输出**——调试要么用界面内 Text，要么 CMake 去掉 WIN32 标志。

---

## 9. 五分钟上手路径

1. 读 [main.cpp](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/src/main.cpp)（40 行）→ 知道 `backend` 怎么进来
2. 读 [Theme.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Theme.qml) → 所有色值/尺寸常量的字典
3. 读 [Main.qml](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/Main.qml) → 整体布局与全局状态
4. 按需深入 [TimelinePane](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/TimelinePane.qml) / [BackupFileListPane](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/BackupFileListPane.qml) / [SearchResultsPane](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/SearchResultsPane.qml) / [SettingsDialog](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/qml/components/SettingsDialog.qml)
5. 需要改后端接口时读 [BackendBridge.h](file:///d:/DATA/GitHub/timemachineplus_cpp/qml-ui/src/BackendBridge.h)（信号/槽清单即 API 清单）
