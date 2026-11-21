# alpOS GUI + PS/2 input + GitHub Actions patch

This overlay replaces the earlier desktop-only prototype. It adds polling-mode PS/2 keyboard initialization (including controller configuration and keyboard scan enable), PS/2 mouse initialization (defaults + reporting enable), mouse packet decoding, and a framebuffer software pointer. It also installs a GitHub Actions workflow that builds the ISO, runs the existing QEMU boot smoke test, and uploads the ISO/kernel/initramfs artifacts.

## Apply

From the repository root after extracting this directory:

```bash
bash alpos-gui-input-ci/tools/apply-gui-stage1.sh .
git add kernel .github tools/ci
 git commit -m "Add alpOS GUI keyboard mouse and CI build"
git push origin main
```

The script copies only the overlay files; it does not replace VFS, ELF loader, userland, or other unrelated repository files.

## GitHub Actions

After the commit is pushed, open the repository's **Actions** tab and select **alpOS x86_64 build**. The workflow installs the cross compiler, GRUB, xorriso, mtools and QEMU; runs `make clean`, `make iso`, and `make test`; then uploads `alpos-x86_64-gui-iso` containing the bootable ISO, kernel ELF and initramfs.

## Input behavior

- Keyboard: PS/2 set-1 translated scan codes are polled; serial input remains available as a fallback.
- Mouse: PS/2 three-byte packets move a software pointer on the framebuffer.
- The current desktop is still a shell-first prototype. Mouse clicks are not yet wired to open Files/Disk windows; this patch fixes pointer movement and keyboard input first.

## Validation and limits

The changed `main.c` and `gfx.c` compile locally with `-Wall -Wextra -Werror`; the input verifier and shell syntax checks pass. A full ISO build/QEMU run cannot be performed in this environment because GRUB/xorriso/QEMU are not installed. The GitHub workflow must run after the patch is committed to verify the complete image. This patch does not modify GitHub by itself.
