# alpOS userspace Stage 2 (ABI draft)

This patch adds a buildable ELF64 userspace skeleton and a `newc` CPIO initramfs packer.

## Current contents

- `userland/bin/echo.c`, `userland/bin/uname.c`: freestanding example programs.
- `userland/lib/start.S`: ELF entry point `_start` calls `main`, then requests exit.
- `userland/include/alpos/syscall.h`: draft x86_64 syscall ABI using `int $0x80`.
- `userland/link.ld`: ELF64 executable linker layout.
- `tools/mknewc.py`: emits SVR4 `newc` CPIO archives.

## Important status

The current kernel does **not** yet implement `int $0x80`, user-mode GDT/TSS setup, ring-3 transitions, process address spaces, argv setup, or ELF loading. These programs are therefore build artifacts only; they cannot yet be launched by the alpOS shell. The syscall numbers and register convention are a proposed ABI and must be frozen together with the kernel implementation before applications rely on them.

## Build

```sh
make -C userland CROSS=x86_64-linux-gnu- verify
make -C userland CROSS=x86_64-linux-gnu- initramfs
```

The CPIO archive is a staging format only. The current boot path does not yet pass it to the kernel.
