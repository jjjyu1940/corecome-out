# alpOS x86_64 top-level build entry points
# Default: compile the freestanding x86_64 kernel ELF.
.DEFAULT_GOAL := all

.PHONY: all x86 x86-build iso x86-iso test x86-test run clean x86-clean help

all: x86-build

x86: x86-build

x86-build:
	$(MAKE) -C kernel -f Makefile.x86 x86-build

iso x86-iso:
	$(MAKE) -C kernel -f Makefile.x86 x86-iso

test x86-test:
	$(MAKE) -C kernel -f Makefile.x86 x86-test

run:
	qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso -serial stdio -monitor none

clean x86-clean:
	$(MAKE) -C kernel -f Makefile.x86 x86-clean

help:
	@echo "alpOS x86_64 targets:"
	@echo "  make          Build kernel ELF"
	@echo "  make iso      Build bootable ISO"
	@echo "  make test     Build ISO and run QEMU boot smoke test"
	@echo "  make run      Run existing ISO in interactive QEMU"
	@echo "  make clean    Remove x86 build output"
