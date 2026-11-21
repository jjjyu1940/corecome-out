# alpOS x86_64 build entry points
.DEFAULT_GOAL := all
.PHONY: all x86 x86-build iso x86-iso test x86-test run clean x86-clean help check-iso

all: x86-build
x86: x86-build
x86-build:
	$(MAKE) -C kernel -f Makefile.x86 x86-build

# ISO is a first-class target; it always builds the ELF first.
iso x86-iso:
	$(MAKE) -C kernel -f Makefile.x86 x86-iso
	@test -s kernel/build-x86/alpos.iso

check-iso: iso
	@file kernel/build-x86/alpos.iso
	@ls -lh kernel/build-x86/alpos.iso

test x86-test:
	$(MAKE) -C kernel -f Makefile.x86 x86-test

run: check-iso
	qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso -serial stdio -monitor none

clean x86-clean:
	$(MAKE) -C kernel -f Makefile.x86 x86-clean

help:
	@echo "alpOS x86_64 targets:"
	@echo "  make          Build kernel ELF"
	@echo "  make iso      Build bootable ISO (kernel/build-x86/alpos.iso)"
	@echo "  make check-iso Build and verify ISO file exists"
	@echo "  make test     Build ISO and run QEMU boot smoke test"
	@echo "  make run      Run ISO in QEMU"
	@echo "  make clean    Remove x86 build output"
