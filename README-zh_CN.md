本文件由模板生成。
除项目名称和程序名称外，其余内容均为占位符。
请根据当前项目的具体情况重写此文档。

# pidload

`pidload` 是一个简单的基于 Meson 的 **C 命令行** 模板（不含共享/静态库打包）。  
`pidload` 是示例应用；可在 `meson.build` 的 `app_sources` 中继续添加应用。

## 仓库结构

- `src/` - 应用源码（`pidload.c`）与小型辅助模块（`commons.c`）
- `tests/` - 最小化单元测试（不依赖 Check）
- `debian/` - Debian 打包元数据
- `docs/` - AsciiDoc man 页源文件
- `meson.build` - 顶层构建定义

## 示例应用：`pidload`

```bash
pidload [OPTION]... [FILE]...
```

类似 `cat`：将文件拼接输出到 stdout。支持 `-v`/`--verbose`、`-q`/`--quiet`、`-h`/`--help`、`--version`。

## 构建

```bash
sudo apt install meson ninja-build gcc pkg-config asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## 许可证

Copyright (C) 2026 Lenik <pidload@bodz.net>

采用 **AGPL-3.0-or-later** 许可。完整文本见 `LICENSE`。
