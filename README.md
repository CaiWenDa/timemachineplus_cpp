# timemachineplus

## timemachineplus 原项目介绍（from epcdiy）

苹果timemachine复刻，超越，可支持本地磁盘数据和局域网拉取备份其他电脑，支持多备份硬盘分布式存储，java开发，全平台支持

许可证：<br>
本源代码许可证基于 GPL v3.
具体见LICENSE文件[here](/LICENSE).

软件的介绍请参考我B站视频：<br>
https://www.bilibili.com/video/BV1Ls4y1c7Wd/

## timemachineplus (C++ port - initial version)

这个目录包含将现有 Java 项目迁移到 C++ 的初始版本。目标采用 C++17 和 CMake 构建系统。

快速开始（仅在 Windows）：

1. 下载 [release](https://github.com/CaiWenDa/timemachineplus_cpp/releases/tag/release) 版本
2. 使用 add 命令添加备份源和目标目录

```shell
timemachineplus add -s /path/to/your/source
timemachineplus add -t /path/to/your/target
```

3. 使用 restore 命令恢复备份
```shell
timemachineplus restore /path/to/your/source/filename
```
4. 使用 rm 命令删除备份源或目标目录

```shell
timemachineplus rm -s /path/to/your/source
timemachineplus rm -t /path/to/your/target
```

5. 列出所有备份源和目标目录
```shell
timemachineplus list
```

6. 无参数运行 timemachineplus 即可实现开始备份

说明：当前为测试版本，功能完整性和稳定性需要进一步测试反馈
