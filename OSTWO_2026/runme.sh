#!/bin/sh
# Direct QEMU boot. The kernel programs the QEMU/Bochs display adapter
# itself, so this gives the full 1024x768x32 desktop without needing
# a GRUB ISO (grub-mkrescue requires xorriso, not installed here).
# For ISO boot instead: install xorriso, then: make iso && make run-iso
make clean && make && make run
