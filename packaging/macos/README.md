# macOS packaging

Native Darwin host:

```sh
make -C packaging/macos
```

writes `packaging/macos/out/<pkg>-<ver>.pkg` via `pkgbuild`.

On non-macOS hosts, use **gh-makerelease** with:

```
<project>/.config/pidload/macos.build-host
$HOME/.config/pidload/macos.build-host
```
