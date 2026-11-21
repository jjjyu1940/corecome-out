# alpOS Ring 3 / ELF userland stage (overlay patch)

This patch targets the current `main` tree. It does not merge itself into GitHub.
Unzip at the repository root and allow the listed files to be added/replaced.
Keep the existing `kernel/arch/x86_64/vfs.c` and `vfs.h` unchanged.

## What this stage implements

- GDT user code/data descriptors and a 64-bit TSS with `RSP0`.
- IDT setup and DPL3 interrupt gate at vector `0x80`.
- A Ring 3 entry/return path using `iretq` and TSS stack switching, with a separate kernel syscall stack.
- `int 0x80` syscall dispatcher: `write(1, user_buffer, length)` and `exit(status)`.
- User/supervisor page-table separation: kernel huge pages are supervisor-only;
  only the user ELF window and user stack use user-accessible 4 KiB PTEs.
- Multiboot2 initramfs module discovery and `newc` CPIO file lookup.
- Minimal ELF64 ET_EXEC PT_LOAD loader for `/bin/echo` and `/bin/uname`.
- Shell fallback that executes `/bin/<command>`; explicit `exec /bin/echo ...` is also supported.
- ISO build now embeds `userland/build/initramfs.cpio` as a GRUB Multiboot2 module.

## Build

On Ubuntu/Debian with the repository's existing toolchain and GRUB dependencies:

```sh
make clean
make iso
```

The image is `kernel/build-x86/alpos.iso`. To boot interactively:

```sh
qemu-system-x86_64 -machine pc -m 256M \
  -cdrom kernel/build-x86/alpos.iso -serial stdio -monitor none
```

At the shell, try:

```text
echo hello from ring3
/bin/echo independent ELF
uname
```

## Important constraints of this first implementation

This is a bring-up implementation, not a general-purpose process environment.
The loader uses fixed physical RAM windows at 32 MiB (up to 2 MiB for the ELF)
and 48 MiB (16 KiB user stack), so use a machine with at least 64 MiB RAM and
ensure those ranges are available. The loader accepts small static ELF64
ET_EXEC files whose PT_LOAD segments fit in `[0x400000, 0x600000)`.
The current syscall set is only `write` and `exit`; there is no scheduler,
preemption, fork, dynamic linker, mmap, signals, user-mode filesystem API,
per-process address space, or disk persistence. Hardware IRQs stay disabled.
The VFS remains volatile RAM only. GCC is still a host-side cross compiler, not
an in-OS compiler.

## Validation boundary

The patch is designed for compile/link/ELF/CPIO checks. A real QEMU boot and
interactive `echo` test are required before claiming runtime success. If the
kernel halts, inspect serial output first. Do not interpret successful ISO
creation alone as proof that Ring 3 works.
