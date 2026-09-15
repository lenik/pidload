# SPDX-License-Identifier: AGPL-3.0-or-later
# FreeBSD pkg: <pkg>-<ver>.pkg
#
# Native FreeBSD: meson + pkg create.
# Other hosts: .config/pidload/freebsd.build-host

include ../lib/common.mk

KIND    := freebsd
OUTDIR  := $(CURDIR)/out
STAGE   := $(CURDIR)/stage
BUILD   := $(CURDIR)/build
MANIFEST := $(CURDIR)/+MANIFEST
ARTIFACT := $(OUTDIR)/$(NAME)-$(VERSION).pkg

.PHONY: all local stage pkg clean

all:
	"$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local

local: $(ARTIFACT)

stage:
	rm -rf "$(STAGE)" "$(BUILD)"
	mkdir -p "$(STAGE)" "$(BUILD)"
	cd "$(SRCDIR)" && meson setup "$(BUILD)" --prefix=/usr/local --buildtype=release
	meson compile -C "$(BUILD)"
	DESTDIR="$(STAGE)" meson install -C "$(BUILD)"

$(MANIFEST):
	@printf '%s\n' \
		'name: $(NAME)' \
		'version: $(VERSION)' \
		'origin: devel/$(NAME)' \
		'comment: Meson-based CLI' \
		'www: https://github.com/lenik/pidload' \
		'maintainer: pidload@bodz.net' \
		'prefix: /usr/local' \
		'desc: Meson-based CLI package' \
		> "$(MANIFEST)"

$(ARTIFACT): stage $(MANIFEST)
	mkdir -p "$(OUTDIR)"
	pkg create -M "$(MANIFEST)" -r "$(STAGE)" -o "$(OUTDIR)"
	@echo "$(ARTIFACT)"

pkg: $(ARTIFACT)

clean:
	rm -rf "$(OUTDIR)" "$(STAGE)" "$(BUILD)" "$(MANIFEST)"
