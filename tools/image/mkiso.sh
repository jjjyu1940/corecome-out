#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KERNEL="$ROOT/kernel"
BUILD="$KERNEL/build-x86"
STAGING="$BUILD/iso-root"
ISO="$BUILD/alpos.iso"
for tool in grub-mkrescue xorriso mformat; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: missing '$tool' (install grub-pc-bin grub-efi-amd64-bin grub-common xorriso mtools)." >&2; exit 127; }
done
test -s "$BUILD/alpos-kernel.elf" || { echo "ERROR: kernel ELF missing" >&2; exit 1; }
test -s "$ROOT/userland/build/initramfs.cpio" || { echo "ERROR: initramfs missing; build userland first" >&2; exit 1; }
rm -rf "$STAGING" "$ISO"
mkdir -p "$STAGING/boot/grub"
install -m 0644 "$BUILD/alpos-kernel.elf" "$STAGING/boot/alpos-kernel.elf"
install -m 0644 "$ROOT/userland/build/initramfs.cpio" "$STAGING/boot/initramfs.cpio"
install -m 0644 "$ROOT/iso/boot/grub/grub.cfg" "$STAGING/boot/grub/grub.cfg"
echo "[mkiso] Creating $ISO"
grub-mkrescue -o "$ISO" "$STAGING" --compress=no -V alpOS
test -s "$ISO"
echo "[mkiso] PASS: $(du -h "$ISO" | cut -f1) $ISO"
