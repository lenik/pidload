THIS FILE IS GENERATED FROM A TEMPLATE.
Except for the project and program names, all content is placeholder text.
Please rewrite this file to reflect the specific details of the current project.

# pidload

`pidload` is a simple Meson-based **C CLI** template (no shared/static library packaging).
`pidload` is one **example app**; more apps can be added via `app_sources` in `meson.build`.

## Repository layout

- `src/` - application sources (`pidload.c`) and small helpers (`commons.c`)
- `tests/` - minimal unit tests (no Check dependency)
- `debian/` - Debian packaging metadata
- `man/` - AsciiDoc man page sources (`man/*.adoc`)
- `meson.build` - top-level build definition

## Example app: `pidload`

```bash
pidload [OPTION]... [FILE]...
```

Cat-like: concatenates files to stdout. Supports `-v`/`--verbose`, `-q`/`--quiet`, `-h`/`--help`, `--version`.

## Build

```bash
sudo apt install meson ninja-build gcc pkg-config asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## License

Copyright (C) 2026 Lenik <pidload@bodz.net>

Licensed under **AGPL-3.0-or-later**.  
See `LICENSE` for the full text and supplemental project terms.
