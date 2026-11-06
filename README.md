# Time Machine+ (C++)

macOS Time Machine 的开源复刻实现（C++17 移植版）：把指定目录增量备份到本地磁盘或多块备份盘，按文件保留历史版本，支持搜索、恢复与完整性校验。

- 双形态：**命令行（CLI）** + **桌面 GUI（Qt Quick / QML）**
- 多源多目标：可同时备份多个源目录，自动按剩余空间选择目标盘
- 版本化存储：每个文件每次备份生成一条历史快照，物理文件按 `md5_时间戳` 命名
- 单文件数据库：全部元数据存于 SQLite（`timemachine.db`），便于迁移与备份
- 跨平台后端：Windows / Linux 均可构建 CLI **与 QML GUI**；Linux 下 QML 使用系统 OpenSSL + pkg-config

> 本项目源自 [epcdiy/timemachineplus](https://github.com/epcdiy/timemachineplus)（Java 版），本仓库为其 C++ 重构版本。

---

## 功能概览

| 功能 | CLI | GUI |
|------|-----|-----|
| 添加/删除备份源、备份目标 | ✅ | ✅ |
| 执行备份 | ✅ | ✅ |
| 浏览备份时间轴与文件列表 | — | ✅ |
| 按文件名搜索历史版本 | — | ✅ |
| 恢复文件 | ✅ | ✅ |
| 数据完整性校验（可选 MD5） | ✅ | — |
| 清理指定源的历史数据 | ✅ | — |
| HTTP REST API | ✅ | — |
| stdio JSON 行协议 | ✅ | — |

---

## 快速开始

### 使用发布包（Windows）

1. 从 [Releases](https://github.com/CaiWenDa/timemachineplus_cpp/releases) 下载绿色包或安装包；
2. 双击 `timemachineplus_qml.exe` 启动图形界面；
3. 在「设置」中添加备份源与备份目标，点击备份即可。

### 命令行快速上手

```shell
# 添加备份源与目标
timemachineplus add -s /path/to/your/source
timemachineplus add -t /path/to/your/target

# 查看配置
timemachineplus list

# 执行一次备份（无参数运行）
timemachineplus

# 恢复文件
timemachineplus restore /path/to/your/source/filename

# 删除源或目标
timemachineplus rm -s /path/to/your/source
timemachineplus rm -t /path/to/your/target
```

---

## 从源码构建

### 依赖

| 依赖 | 要求 |
|------|------|
| C++ 编译器 | C++17（MSVC 2022/2026 x64 或 GCC） |
| CMake | ≥ 3.10（使用 `CMakePresets.json` 需 ≥ 3.21） |
| Qt（可选，构建 GUI） | Qt 5.15（Quick / Qml / Gui / QuickControls2 / Concurrent） |
| OpenSSL | Windows 使用内置预编译库；Linux / macOS 使用系统库（`libssl-dev` / `openssl-devel`） |
| 其他 | SQLiteCpp、cpp-httplib、nlohmann/json 均已内置，无需额外安装 |

初始化子模块（首次克隆后）：

```shell
git submodule update --init --recursive
```

### Windows

```powershell
# 仅 CLI
cmake --preset x64-Release
cmake --build --preset x64-Release

# CLI + QML 界面
$env:QT_DIR = "C:\Qt\5.15.2\msvc2019_64"
cmake --preset x64-Release-QML
cmake --build --preset x64-Release-QML
```

产物位置：

- CLI：`out/build/x64-Release/Release/timemachineplus.exe`
- GUI：`build-release/qml-ui/Release/timemachineplus_qml.exe`

### Linux

```shell
# 安装依赖（Debian / Ubuntu）
sudo apt install -y build-essential cmake ninja-build pkg-config \
    libssl-dev qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev

# 仅 CLI
cmake --preset Linux-GCC-Release
cmake --build --preset Linux-GCC-Release

# CLI + QML UI
export QT_DIR=/opt/Qt/5.15.2/gcc_64       # Qt 非系统默认路径时需设置
cmake --preset Linux-GCC-Release-QML
cmake --build --preset Linux-GCC-Release-QML
```

> Linux 构建会使用系统 OpenSSL（`libssl-dev`），不依赖仓库里的 `thirdparty/OpenSSL/lib/libcrypto.a`（该文件为 macOS 预编译产物）。

### 一键打包（Windows）

```powershell
powershell -ExecutionPolicy Bypass -File packaging\package.ps1
```

产出：

- `dist/` — 可直接双击运行的绿色目录（含 Qt 运行库、OpenSSL、schema）
- `dist-installer/` — 安装包 `TimeMachinePlus-<版本>-x64-setup.exe`（需安装 Inno Setup 6）

完整的环境配置、预设说明、产物清单与故障排查见 **[项目技术文档](docs/PROJECT_TECHNICAL_DOC.md)**。

---

## CLI 命令

| 命令 | 说明 |
|------|------|
| `timemachineplus` | 执行一次全量备份 |
| `timemachineplus add -s <路径>` | 添加备份源 |
| `timemachineplus add -t <路径>` | 添加备份目标 |
| `timemachineplus rm -s <路径>` / `rm -t <路径>` | 删除源 / 目标 |
| `timemachineplus list` | 列出所有源与目标 |
| `timemachineplus restore <源文件路径>` | 恢复文件的最新版本 |
| `timemachineplus checkdata` | 校验备份完整性（不校验哈希） |
| `timemachineplus checkdatawithhash` | 校验完整性并校验 MD5 |
| `timemachineplus <rootid>` | 删除指定备份源的全部历史数据 |
| `timemachineplus ui [port]` | 启动 HTTP REST API（默认 8765，仅 127.0.0.1） |
| `timemachineplus --stdio` | 启动 stdin/stdout JSON 行协议服务 |

---

## 数据与配置

| 内容 | 位置 |
|------|------|
| 数据库 | CLI：当前目录 `timemachine.db`；GUI 安装态：`%LOCALAPPDATA%\TimeMachinePlusData\timemachine.db` |
| 日志 | CLI：当前目录 `log.log`；GUI：exe 同目录 `log.log` |
| schema | `timemachine.sql`（随包分发，启动时执行，幂等） |
| 备份数据 | `<备份目标根目录>/BACKUPDATABASE/<md5>_<时间戳>` |

可用环境变量覆盖：

| 变量 | 作用 |
|------|------|
| `TIMEMACHINE_DB` | 指定数据库绝对路径 |
| `TIMEMACHINE_SCHEMA` | 指定 schema 文件绝对路径 |
| `QT_DIR` | CMake 定位 Qt 5.15 安装目录（使用 QML 预设时） |

---

## 项目结构

```
├── include/            # 公共头文件（config / models / service_run / sqlite_helper / util）
├── src/                # 后端实现（CLI 入口、备份引擎、HTTP API、stdio 协议）
├── qml-ui/             # Qt Quick 桌面界面（Main.qml、Theme.qml、components）
│   └── src/            # QML 入口与 BackendBridge（QML ↔ C++ 桥接）
├── thirdparty/         # SQLiteCpp / cpp-httplib / nlohmann-json / OpenSSL
├── packaging/          # package.ps1 / installer.iss
├── docs/               # 技术文档与 QML 教程
├── CMakeLists.txt      # 根构建脚本
└── CMakePresets.json   # 构建 / 打包预设
```

技术架构、模块职责、数据模型与二次开发指引详见 [docs/PROJECT_TECHNICAL_DOC.md](docs/PROJECT_TECHNICAL_DOC.md)。

QML 学习资料：

- [CSS 开发者的 QML 布局教程](docs/QML_LAYOUT_TUTORIAL_FOR_CSS_DEV.md)
- [QWidget 开发者的 QML 指南](docs/qml-guide-for-qwidget-devs.md)

---

## 许可证

本项目基于 **GPL v3** 许可证，详见 [LICENSE](LICENSE)。

## 相关链接

- 原 Java 版项目：<https://github.com/epcdiy/timemachineplus>
- 项目介绍视频：<https://www.bilibili.com/video/BV1Ls4y1c7Wd/>
