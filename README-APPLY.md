# alpOS Unix bring-up — stage 1

This is an overlay for `jjjyu1940/corecome-out` (main), not a complete OS or a replacement for the repository.

## Files
- Replace `kernel/arch/x86_64/main.c`.
- Add `kernel/arch/x86_64/vfs.h` and `kernel/arch/x86_64/vfs.c`.
- Update `kernel/Makefile.x86` to compile `vfs.c` and link `build-x86/vfs.o` into the kernel.

Add these Makefile rules:

```make
$(BUILD)/vfs.o: $(ARCH)/vfs.c $(ARCH)/vfs.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/alpos-kernel.elf: $(BUILD)/start.o $(BUILD)/main.o $(BUILD)/vfs.o $(ARCH)/boot/link.ld
	$(LD) -T $(ARCH)/boot/link.ld -nostdlib -z max-page-size=0x1000 -o $@ $(BUILD)/start.o $(BUILD)/main.o $(BUILD)/vfs.o
```

## Current functionality
- Polling PS/2 set-1 keyboard input, while retaining COM1 serial input.
- Basic shell commands: `help`, `about`, `clear`, `mem`, `ls`, `pwd`, `cd`, `mkdir`, `touch`, `cat`, `reboot`, `halt`.
- In-memory VFS with `/bin`, `/sbin`, `/boot`, `/dev`, `/etc`, `/home`, `/lib`, `/proc`, `/root`, `/tmp`, `/usr/{bin,include,lib,share}`, `/var`.
- `/etc/os-release` seed file.

## Limitations (important)
This is a kernel-resident RAM VFS only. It is volatile and has no disk persistence, permissions, mount API, symlinks, block driver, userspace, process model, system calls, or ELF user-program loader. The shell commands are still kernel code, not separate executable files. This is intentionally a foundation, not a claim of POSIX compatibility. The PS/2 driver is polling-only and assumes a legacy PS/2 controller; serial remains available as a fallback.

## Build
Run the existing CI workflow or `make clean && make iso`. Verify the serial boot smoke test and test keyboard input in QEMU with a graphical display. Do not claim hardware keyboard support from the serial-only CI smoke test.
