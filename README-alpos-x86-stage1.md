# alpOS x86_64 bring-up image

This patch replaces the broken/incomplete x86_64 boot path with a small, honest first milestone:

- GRUB 2 + a valid Multiboot2 header
- 32-bit protected-mode entry, page tables, and transition to x86_64 long mode
- 1 GiB identity map using 2 MiB pages
- serial (COM1) and VGA text output
- an interactive serial shell with `help`, `about`, `clear`, `mem`, `reboot`, and `halt`
- a reproducible ISO build and a QEMU boot smoke test

It is **not yet a complete desktop OS**. There is no user-mode process model, disk driver, filesystem, networking, package manager, or GUI in this milestone. The existing AArch64 kernel remains separate and is not reused as x86 code.

## Build on Ubuntu 24.04

```sh
sudo apt-get update
sudo apt-get install -y gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  grub-pc-bin grub-efi-amd64-bin grub-common xorriso qemu-system-x86
make -C kernel -f Makefile.x86 x86-iso
```

Output: `kernel/build-x86/alpos.iso`

## Run in QEMU

```sh
qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso \
  -serial stdio -monitor none
```

The shell is on the serial console. Type `help` to see commands.

## Automated validation

```sh
make -C kernel -f Makefile.x86 x86-test
```

The target verifies the ELF machine type and Multiboot2 header, inspects the ISO's El Torito boot record and required files, then boots the ISO in QEMU and requires the serial marker `[alpOS] shell ready`. A timeout is expected because the shell intentionally waits for input.

GitHub Actions workflow: `.github/workflows/alpos-x86-iso.yml`. It uploads `alpos.iso` and the serial boot log as the `alpos-x86_64-iso` artifact.
