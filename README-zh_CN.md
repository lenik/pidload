# pidload

`pidload` 是面向 Linux 的 **wxWidgets** 进程与流量监视器。
它在滑动时间窗口中绘制所选进程的 CPU/I/O，以及可选的块设备、网络、系统
CPU/内存曲线，支持可停靠面板，并可按采样间隔写入日志。

## 用法

```bash
pidload [OPTION]... [NAME]...
```

每个 `NAME` 按以下方式匹配进程：pid、可执行名、路径、窗口标题，或通配符
（`*`、`?`）。例如 `pidload editor` 可匹配 `/bin/editor` 以及标题为
“Some Editor” 的窗口。

多个进程使用不同线型（共 10 种，循环复用）；进程图有独立的进程图例可点击开关。

### 选项

| 选项 | 含义 |
|------|------|
| `-d`, `--dev DEVICE` | 块设备 I/O |
| `-i`, `--iface NETDEV`/`all` | 网卡 |
| `-a`, `--addr NETADDR` | 按地址 |
| `-c`, `--cpu` | 系统 CPU（总体与每核） |
| `-m`, `--memory` | 系统内存与交换 |
| `-T`, `--threads` | 线程计数（存活 / 等待 / 累计打开） |
| `-F`, `--numfd` | 打开的文件描述符数 |
| `-C`, `--connections` | 网络连接（存活 / 等待 / 累计） |
| `-t`, `--interval D` | 采样间隔（默认 `2s`） |
| `-w`, `--window D` | 默认可视窗口（默认 `3min`）；会话历史保存在内存中 |
| `-o`, `--output PATH` | 日志目录（`name.pid.log`） |
| `-v` / `-q` / `-h` / `--version` | 详细、安静、帮助、版本 |

在任一图表上拖动可平移共享时间视图，滚轮缩放（所有图表同步）。至少有一个图表
在 X 轴显示经过时间标签。

**View → Display Curve** 可选 Segment / Bezier / Bicubic 插值。
网络图右键菜单 **Display Unit** 可切换原始字节、载荷估算或包数量。

视图布局与开关会按 `NAME` 组合记住，保存在
`~/.config/pidload/<sha1>.state`（密钥为分类后的 `path`/`pid`/`glob` 的 SHA-1）。
显式命令行选项会在本次运行中覆盖已保存的值。

## 构建

```bash
sudo apt install meson ninja-build g++ pkg-config libbas-c-dev libwxgtk3.2-dev libx11-dev libssl-dev asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## 许可证

Copyright (C) 2026 Lenik <pidload@bodz.net>

采用 **AGPL-3.0-or-later** 许可。完整文本见 `LICENSE`。
