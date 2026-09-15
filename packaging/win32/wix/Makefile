# SPDX-License-Identifier: AGPL-3.0-or-later
# WiX MSI: <pkg>-<ver>.msi
#
# Native Windows: build.cmd → build.ps1 (cmd.exe / PowerShell).
# Linux: optional wixl; otherwise .build-host to Windows.
# Remote Windows: host.sh runs packaging\win32\wix\build.cmd via cmd/ps.

include ../../lib/common.mk

KIND    := wix
OUTDIR  := $(CURDIR)/out
STAGE   := $(CURDIR)/stage
WXS     := $(CURDIR)/product.wxs
FILES   := $(CURDIR)/files.wxs
ARTIFACT := $(OUTDIR)/$(NAME)-$(VERSION).msi

.PHONY: all local wixl clean

all:
	"$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local

local:
ifeq ($(OS),Windows_NT)
	cmd.exe /c "$(CURDIR)/build.cmd" -Name "$(NAME)" -Version "$(VERSION)" -SrcDir "$(SRCDIR)"
else
	@$(MAKE) -C "$(CURDIR)" wixl
endif

wixl: $(ARTIFACT)

$(ARTIFACT): $(WXS)
	mkdir -p "$(OUTDIR)" "$(STAGE)"
	@set -e; \
	if [ -d "$(CURDIR)/../mingw/stage" ] && [ -n "$$(find "$(CURDIR)/../mingw/stage" -type f 2>/dev/null | head -1)" ]; then \
		rm -rf "$(STAGE)" && mkdir -p "$(STAGE)" && cp -a "$(CURDIR)/../mingw/stage/." "$(STAGE)/"; \
	elif "$(HOST_SH)" can-local mingw; then \
		$(MAKE) -C "$(CURDIR)/../mingw" local; \
		rm -rf "$(STAGE)" && mkdir -p "$(STAGE)" && cp -a "$(CURDIR)/../mingw/stage/." "$(STAGE)/"; \
	else \
		echo "wix (wixl): need ../mingw/stage first" >&2; \
		exit 1; \
	fi; \
	if command -v heat >/dev/null 2>&1; then \
		heat dir "$(STAGE)" -gg -sfrag -srd -cg AppFiles -dr INSTALLDIR \
			-var var.StageDir -out "$(FILES)"; \
	else \
		python3 - "$(STAGE)" "$(FILES)" <<'PY'
import sys, uuid
from pathlib import Path
stage, out = Path(sys.argv[1]), Path(sys.argv[2])
ns = "http://schemas.microsoft.com/wix/2006/wi"
comps = []
for path in sorted(stage.rglob("*")):
    if not path.is_file():
        continue
    rel = path.relative_to(stage).as_posix()
    cid = "c" + uuid.uuid5(uuid.NAMESPACE_URL, rel).hex[:16]
    comps.append(
        f'    <Component Id="{cid}" Directory="INSTALLDIR" Guid="*">\n'
        f'      <File Id="{cid}f" Source="$(var.StageDir)\\{rel.replace("/", "\\")}" KeyPath="yes" />\n'
        f'    </Component>'
    )
body = "\n".join(comps) or (
    '    <Component Id="Placeholder" Directory="INSTALLDIR" Guid="*">\n'
    '      <CreateFolder />\n'
    '    </Component>'
)
out.write_text(
    '<?xml version="1.0" encoding="utf-8"?>\n'
    f'<Wix xmlns="{ns}">\n'
    '  <Fragment>\n'
    '    <ComponentGroup Id="AppFiles">\n'
    f"{body}\n"
    "    </ComponentGroup>\n"
    "  </Fragment>\n"
    "</Wix>\n",
    encoding="utf-8",
)
PY
	fi; \
	if command -v wixl >/dev/null 2>&1; then \
		wixl -D PkgName=$(NAME) -D PkgVersion=$(VERSION) \
			-D StageDir="$(STAGE)" \
			-o "$(ARTIFACT)" "$(WXS)" "$(FILES)"; \
	else \
		echo "wix: on Unix use wixl, or build-host to a Windows machine (cmd/ps)" >&2; \
		exit 1; \
	fi
	@echo "$(ARTIFACT)"

clean:
	rm -rf "$(OUTDIR)" "$(STAGE)" "$(CURDIR)/build" "$(FILES)" "$(CURDIR)"/*.wixobj
