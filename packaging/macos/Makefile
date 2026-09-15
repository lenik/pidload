# macOS packaging
#
# Native Darwin: meson install into stage, then pkgbuild.
# Other hosts: $HOME/.config/pidload/macos.build-host (see packaging/lib/host.sh).

include ../lib/common.mk

KIND    := macos
OUTDIR  := $(CURDIR)/out
STAGE   := $(CURDIR)/stage
BUILD   := $(CURDIR)/build
IDENTIFIER ?= net.bodz.$(NAME)
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

$(ARTIFACT): stage
	mkdir -p "$(OUTDIR)"
	pkgbuild --root "$(STAGE)/usr/local" \
		--identifier "$(IDENTIFIER)" \
		--version "$(VERSION)" \
		"$(ARTIFACT)"
	@echo "$(ARTIFACT)"

pkg: $(ARTIFACT)

clean:
	rm -rf "$(OUTDIR)" "$(STAGE)" "$(BUILD)"
