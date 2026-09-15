# SPDX-License-Identifier: AGPL-3.0-or-later
# Arch Linux package via makepkg.
#
# Native Arch: makepkg -f (writes <pkg>-<ver>-*.pkg.tar.* under out/).
# Other hosts: gh-makerelease + arch.build-host

include ../lib/common.mk

KIND    := arch
OUTDIR  := $(CURDIR)/out
PKGBUILD := $(CURDIR)/PKGBUILD

.PHONY: all local pkg clean

all:
	"$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local

local: pkg

pkg:
	mkdir -p "$(OUTDIR)"
	@set -e; \
	ver=$(VERSION); \
	sed -i.bak -e "s/^pkgver=.*/pkgver=$$ver/" "$(PKGBUILD)" && rm -f "$(PKGBUILD).bak"; \
	cd "$(CURDIR)" && SRCDEST="$(SRCDIR)" PKGDEST="$(OUTDIR)" makepkg -f --skipchecksums
	@echo "$(OUTDIR)/"

clean:
	rm -rf "$(OUTDIR)" "$(CURDIR)/pkg" "$(CURDIR)/src" "$(CURDIR)"/*.pkg.tar.*
