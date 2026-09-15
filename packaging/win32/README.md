# Windows (win32) packaging

| Target | Directory | Host shell | Artifact |
|--------|-----------|------------|----------|
| mingw-w64 portable | `mingw/` | bash / MSYS | `<pkg>_mingw-<ver>.exe` |
| Inno Setup installer | `innosetup/` | **cmd.exe / PowerShell** | `<pkg>-<ver>.exe` |
| WiX MSI | `wix/` | **cmd.exe / PowerShell** | `<pkg>-<ver>.msi` |

```bat
cd packaging\win32\innosetup
build.cmd
```

```powershell
cd packaging\win32\wix
.\build.ps1 -Name pidload -Version 1.2.3
```

From a Unix checkout, `make -C packaging/win32` builds locally when tools
exist (mingw-w64 / wine / wixl). Remote Windows builds use **gh-makerelease**
with a `.build-host` file (not `host.sh`).

### Local / cross

- **mingw**: MinGW/MSYS, or `x86_64-w64-mingw32-gcc`
- **innosetup** (Windows): `build.cmd` → `build.ps1` → `ISCC.exe`
- **innosetup** (Unix cross): `wine` + `ISCC.exe` (`INNOSETUP_DIR`)
- **wix** (Windows): `build.cmd` → `build.ps1` → WiX 4 (`wix`) or WiX 3 (`candle`/`light`)
- **wix** (Unix): optional `wixl`; else remote via gh-makerelease

### `.build-host` (gh-makerelease)

```
<project>/.config/pidload/<packaging>.build-host
$HOME/.config/pidload/<packaging>.build-host
```

`<packaging>` is `mingw`, `innosetup`, or `wix`, then `win32`.

```
# jump
name: bastion
host: jump.example.com:22
user: ops
identity: ~/.ssh/id_ed25519

name: winbuild
host: 10.0.0.8
user: builder
shell: powershell
build_dir: C:/build/pidload
```

`shell` for the **last** hop: `bash` (default), `cmd`, or `powershell`/`ps`.
Inno Setup and WiX default to `powershell` when `shell` is omitted.

Blank lines separate hosts (`ssh -J` hops). Only the last `build_dir` is used.
See `build-host.example`.
