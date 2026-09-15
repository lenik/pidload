# SPDX-License-Identifier: AGPL-3.0-or-later
# Inno Setup installer: <pkg>-<ver>.exe
#
# Native Windows: build.cmd → build.ps1 (cmd.exe / PowerShell).
# Linux cross: wine + ISCC.exe (INNOSETUP_DIR).
# Remote Windows: gh-makerelease runs packaging\win32\innosetup\build.cmd via cmd/ps.

include ../../lib/common.mk

KIND    := innosetup
OUTDIR  := $(CURDIR)/out
STAGE   := $(CURDIR)/stage
ISS     := $(CURDIR)/setup.iss
ARTIFACT := $(OUTDIR)/$(NAME)-$(VERSION).exe

.PHONY: all local wine clean

all:
	"$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local

# Windows host: prefer cmd → PowerShell. Else wine cross on Unix.
local:
ifeq ($(OS),Windows_NT)
	cmd.exe /c "$(CURDIR)/build.cmd" -Name "$(NAME)" -Version "$(VERSION)" -SrcDir "$(SRCDIR)"
else
	@$(MAKE) -C "$(CURDIR)" wine
endif

wine: $(ARTIFACT)

$(ARTIFACT): $(ISS)
	mkdir -p "$(OUTDIR)" "$(STAGE)"
	@set -e; \
	if [ -d "$(CURDIR)/../mingw/stage" ] && [ -n "$$(find "$(CURDIR)/../mingw/stage" -type f 2>/dev/null | head -1)" ]; then \
		rm -rf "$(STAGE)" && mkdir -p "$(STAGE)" && cp -a "$(CURDIR)/../mingw/stage/." "$(STAGE)/"; \
	elif "$(HOST_SH)" can-local mingw; then \
		$(MAKE) -C "$(CURDIR)/../mingw" local; \
		rm -rf "$(STAGE)" && mkdir -p "$(STAGE)" && cp -a "$(CURDIR)/../mingw/stage/." "$(STAGE)/"; \
	else \
		echo "innosetup (wine): need ../mingw/stage first" >&2; \
		exit 1; \
	fi; \
	if command -v wine >/dev/null 2>&1; then \
		exe=$$( \
			for d in \
				$${INNOSETUP_DIR:+"$$INNOSETUP_DIR"} \
				"$$HOME/.wine/drive_c/Program Files (x86)/Inno Setup 6" \
				"$$HOME/.wine/drive_c/Program Files/Inno Setup 6" \
				/opt/inno-setup; do \
				[ -f "$$d/ISCC.exe" ] && { echo "$$d/ISCC.exe"; break; }; \
			done \
		); \
		if [ -z "$$exe" ]; then \
			echo "innosetup: wine found but ISCC.exe missing (set INNOSETUP_DIR)" >&2; \
			exit 1; \
		fi; \
		wine "$$exe" "$(ISS)" \
			"/DMyAppName=$(NAME)" \
			"/DMyAppVersion=$(VERSION)" \
			"/DMyStage=$(STAGE)"; \
	else \
		echo "innosetup: on Unix use wine+ISCC, or build-host to a Windows machine" >&2; \
		exit 1; \
	fi
	@test -f "$(ARTIFACT)" || { echo "innosetup: missing $(ARTIFACT)" >&2; exit 1; }
	@echo "$(ARTIFACT)"

clean:
	rm -rf "$(OUTDIR)" "$(STAGE)" "$(CURDIR)/build"
