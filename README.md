# corecome-out / alpOS

This repository contains multiple experimental kernel and service tracks. The AArch64 bare-metal kernel and the x86_64 alpOS bring-up are separate targets; do not treat an AArch64 test result as proof that the x86_64 image works.

## x86_64 alpOS boot image

The x86_64 target is currently a **bring-up milestone**, not a finished general-purpose operating system. It boots through GRUB 2 / Multiboot2, enters 64-bit long mode, initializes a minimal identity map, writes to VGA and COM1, and provides a small serial command shell.

Install the Ubuntu 24.04 build dependencies:

```sh
sudo apt-get update
sudo apt-get install -y gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  grub-pc-bin grub-efi-amd64-bin grub-common xorriso qemu-system-x86
```

Build the ISO:

```sh
make -C kernel -f Makefile.x86 x86-iso
```

Run it in QEMU with an interactive serial shell:

```sh
qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso \
  -serial stdio -monitor none
```

Run the build, ISO validation, and QEMU boot smoke test:

```sh
make -C kernel -f Makefile.x86 x86-test
```

The ISO is written to `kernel/build-x86/alpos.iso`. GitHub Actions can produce the same ISO as an artifact using the `Build alpOS x86_64 ISO` workflow.

### Current x86_64 scope

- GRUB / Multiboot2 boot and 32-bit to 64-bit transition
- 2 MiB page mappings for the first 1 GiB
- VGA text output and 115200-baud COM1 serial console
- serial shell commands: `help`, `about`, `clear`, `mem`, `reboot`, `halt`

Not implemented yet: ring-3 user processes, scheduler, disk and filesystem drivers, networking, persistent storage, graphics desktop, and Linux/POSIX compatibility. These must be added and tested before calling alpOS a daily-use OS.

## AArch64 kernel and services

The existing AArch64 kernel and Java service tests remain independent. Use the repository's `make build`, `make test`, `make kernel-test`, and the documentation under `docs/` and `kernel/` for those targets.

## License

AGPL-3.0-only. See [LICENSE](LICENSE).
