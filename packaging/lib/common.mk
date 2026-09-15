# SPDX-License-Identifier: AGPL-3.0-or-later
# Shared NAME / VERSION / host dispatch. Include from packaging/*/Makefile.
#
# _LIBDIR is packaging/lib/; SRCDIR is the project root.

_LIBDIR  := $(dir $(lastword $(MAKEFILE_LIST)))
HOST_SH  := $(abspath $(_LIBDIR)/host.sh)
SRCDIR   ?= $(abspath $(_LIBDIR)/../..)
PACKAGEDIR ?= $(abspath $(_LIBDIR)/..)

NAME    ?= pidload
VERSION := $(shell cd "$(SRCDIR)" && { zfr version 2>/dev/null || true; })
ifeq ($(strip $(VERSION)),)
VERSION := $(shell head -n1 "$(SRCDIR)/VERSION" 2>/dev/null)
endif
VERSION := $(patsubst v%,%,$(strip $(VERSION)))
ifeq ($(strip $(VERSION)),)
VERSION := 0.0.0
endif

export PIDLOAD_SRCDIR := $(SRCDIR)

# Wrap a local recipe: runs here if native/cross tools exist.
# Remote .build-host builds are done by gh-makerelease, not host.sh.
# Usage: "$(HOST_SH)" run $(KIND) -- $(MAKE) -C "$(CURDIR)" local
