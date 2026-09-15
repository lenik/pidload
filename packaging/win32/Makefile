# SPDX-License-Identifier: AGPL-3.0-or-later
# Windows (win32) packaging.
#
#   make -C packaging/win32           # mingw + innosetup + wix
#   make -C packaging/win32 mingw      # <pkg>_mingw-<ver>.exe
#   make -C packaging/win32 innosetup  # <pkg>-<ver>.exe
#   make -C packaging/win32 wix        # <pkg>-<ver>.msi
#
# If this host cannot build a target, packaging/lib/host.sh reads
#   <project>/.config/pidload/<kind>.build-host
#   $HOME/.config/pidload/<kind>.build-host
# (kind: mingw | innosetup | wix, then fallback win32).

include ../lib/common.mk

.PHONY: all mingw innosetup wix clean

all: mingw innosetup wix

mingw:
	$(MAKE) -C mingw

innosetup:
	$(MAKE) -C innosetup

wix:
	$(MAKE) -C wix

clean:
	$(MAKE) -C mingw clean
	$(MAKE) -C innosetup clean
	$(MAKE) -C wix clean
