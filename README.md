# timemachineplus (C++ port - initial skeleton)

这个目录包含将现有 Java 项目迁移到 C++ 的初始骨架。目标采用 C++17 和 CMake 构建系统。

快速开始（在 Windows PowerShell）：

```powershell
# 从仓库根目录执行：
cmake -S cpp -B cpp/build
cmake --build cpp/build --config Release
.
```

说明：当前为最小示例，主要包含 model 的 C++ 等价结构和一个示例 main。下一步会迁移数据库辅助和主逻辑。
