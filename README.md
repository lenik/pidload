# pidload

`pidload` is a **wxWidgets** process and traffic monitor for Linux.
It charts CPU/I/O for selected processes and optional block-device / network /
system CPU / memory traffic in a sliding window, with dockable panes and
optional per-interval log recording.

## Usage

```bash
pidload [OPTION]... [NAME]...
```

Each `NAME` matches processes as: pid, executable name, path, window title,
or globs (`*`, `?`). Example: `pidload editor` matches `/bin/editor` and a
window titled “Some Editor”.

Multiple processes get distinct line styles (10 styles, then reused).
Process charts have a separate process legend for toggling.

### Options

| Option | Meaning |
|--------|---------|
| `-d`, `--dev DEVICE` | Block device I/O (repeatable) |
| `-i`, `--iface NETDEV`/`all` | Netdev(s) |
| `-a`, `--addr NETADDR` | Address traffic |
| `-c`, `--cpu` | System CPU (overall + cores) |
| `-m`, `--memory` | System memory + swap |
| `-T`, `--threads` | Thread counts (alive / wait / total opened) |
| `-F`, `--numfd` | Open file-descriptor counts |
| `-C`, `--connections` | Net connections (alive / wait / total) |
| `-t`, `--interval D` | Sample interval (default `2s`) |
| `-w`, `--window D` | Default visible window (default `3min`); session history is kept in memory |
| `-o`, `--output PATH` | Directory for logs (`name.pid.log`) |
| `-v` / `-q` / `-h` / `--version` | Verbose, quiet, help, version |

Drag on a chart to pan the shared time view; mouse wheel zooms (all charts stay
in sync). At least one chart shows elapsed-time labels on the X axis.

**View → Display Curve** chooses Segment / Bezier / Bicubic interpolation.
On a network chart, the context menu **Display Unit** switches raw size,
payload size (wire minus Ethernet header estimate), or packet counts.

### Recording

With `-o /tmp/prefix`:

```
/tmp/prefix/editor.1234.log
; cpu_pct read write
12.50 4096 0
...
```

## Build

```bash
sudo apt install meson ninja-build g++ pkg-config libbas-c-dev libwxgtk3.2-dev libx11-dev asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## License

Copyright (C) 2026 Lenik <pidload@bodz.net>

Licensed under **AGPL-3.0-or-later**.  
See `LICENSE` for the full text and supplemental project terms.
