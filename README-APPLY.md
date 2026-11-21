# alpOS Graphical Desktop Stage 1

This is a standalone overlay for `jjjyu1940/corecome-out` (`main`). It is NOT merged into GitHub.

## Apply

From the repository root, copy the overlay contents into the same paths, allowing replacement of:
- `kernel/arch/x86_64/main.c`
- `kernel/arch/x86_64/boot/start.S`
- `kernel/Makefile.x86`

Add:
- `kernel/arch/x86_64/gui/gfx.c`
- `kernel/arch/x86_64/gui/gfx.h`

Then run:

```sh
make clean
make iso
```

Boot the ISO in Limbo with a video adapter that exposes a Multiboot2 RGB framebuffer. Prefer 1024x768, 32-bit color.

## What this stage does

- Requests a 1024x768x32 Multiboot2 framebuffer.
- Starts directly into an alpOS desktop before showing the command prompt.
- Draws a desktop background, Files / Disk / Terminal icons, a terminal window and taskbar.
- Routes kernel console output into the graphical terminal while retaining serial output.
- Retains the existing shell and PS/2 keyboard polling input.
- Falls back to VGA text output if GRUB does not provide a supported RGB framebuffer.

## Important limitations

This is the first graphical boot stage, not a complete desktop environment. The desktop icons are visual placeholders; mouse pointer/window dragging and click actions are not implemented yet. Keyboard input still goes to the terminal shell. The existing VFS is volatile RAM only. **FAT32, IDE/AHCI disk drivers, partition discovery, and persistent disk writes are not implemented by this overlay.** The Disk icon is a placeholder for the next storage stage.

The framebuffer code currently supports Multiboot2 RGB, 32-bpp framebuffers whose physical base is below 4 GiB. The kernel's current identity map covers the first 4 GiB. Limbo/QEMU boot validation is still required.

## GitHub Actions ISO build

This overlay also adds `.github/workflows/alpos-build.yml`. After applying the overlay and committing/pushing the changes to `main`, GitHub Actions will build the kernel, userland initramfs and bootable ISO on pushes and pull requests. You can also run it manually from the repository's Actions tab using **alpOS x86_64 build** → **Run workflow**.

When the workflow succeeds, open that run and download the artifact named `alpos-x86_64-iso-<commit>`. The artifact contains `alpos.iso`, the kernel ELF and the initramfs. The workflow does not push binaries back into the repository.

The workflow builds the current repository contents; therefore apply this complete overlay before committing the changes. It is not enough to add only the workflow file.
