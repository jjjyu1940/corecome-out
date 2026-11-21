# alpOS x86_64 Stage 3 — restore the Multiboot2 boot path

This is an overlay patch for the current `main` branch. It restores the two files
referenced by `kernel/Makefile.x86` but absent from the tracked x86_64 boot tree:

- `kernel/arch/x86_64/boot/start.S`
- `kernel/arch/x86_64/boot/link.ld`

## Apply

Unzip at the repository root and merge the `kernel/` directory. Do not delete or
replace the existing `main.c`, `vfs.c`, `vfs.h`, or Makefile.

Then run:

```sh
make clean
make
make iso
```

The existing workflow expects the kernel ELF at
`kernel/build-x86/alpos-kernel.elf` and the ISO at `kernel/build-x86/alpos.iso`.

## What this boot code does

- Provides a valid Multiboot2 header in the first 32 KiB of the ELF.
- Accepts the Multiboot2 boot magic and saves the boot information pointer.
- Builds identity and high-half mappings for the first 4 GiB using 2 MiB pages.
- Enables PAE, long mode, and paging, then jumps to the high-half alias.
- Loads a high-half kernel stack and calls `kernel_main(magic, mbi_phys)`.

## Important limits

This restores the boot path; it does **not** implement Ring 3, TSS, interrupt
handling, system-call dispatch, an ELF user-process loader, or initramfs mounting.
The second argument remains the physical Multiboot information address, matching
the current kernel entry signature. The current VFS remains RAM-only.

The local verification used a freestanding x86_64 stub for `kernel_main` to test
assembly, linking, ELF type, entry address, and Multiboot2 checksum. QEMU and
`grub-mkrescue` were not available in the build container, so actual VM/ISO boot
is not claimed here.
