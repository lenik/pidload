# SPDX-License-Identifier: AGPL-3.0-or-later
# Portable MinGW-w64 build: <pkg>_mingw-<ver>.exe
#
# pidload is Linux-only (/proc, X11/wxGTK). Cross builds are not supported;
# this target succeeds as a no-op so zfr package can continue.

include ../../lib/common.mk

KIND    := mingw
OUTDIR  := $(CURDIR)/out
STAGE   := $(CURDIR)/stage
BUILD   := $(CURDIR)/build

.PHONY: all local stage exe clean

all:
	"$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local

local:
	@echo "mingw: skipped — pidload is Linux-only (/proc, X11)" >&2
	@mkdir -p "$(OUTDIR)"
	@printf 'skipped\n' > "$(OUTDIR)/.skipped"

stage: local

clean:
	rm -rf "$(OUTDIR)" "$(STAGE)" "$(BUILD)"
