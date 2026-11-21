#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KERNEL="$ROOT/kernel"
BUILD="$KERNEL/build-x86"
STAGING="$BUILD/iso-root"
ISO="$BUILD/alpos.iso"

for tool in grub-mkrescue xorriso; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required ISO tool '$tool' is missing." >&2
    echo "Ubuntu/Debian: sudo apt-get install grub-pc-bin grub-efi-amd64-bin grub-common xorriso" >&2
    exit 127
  fi
done
if [[ ! -s "$BUILD/alpos-kernel.elf" ]]; then
  echo "ERROR: $BUILD/alpos-kernel.elf is missing; run 'make' first." >&2
  exit 1
fi

rm -rf "$STAGING" "$ISO"
mkdir -p "$STAGING/boot/grub"
install -m 0644 "$BUILD/alpos-kernel.elf" "$STAGING/boot/alpos-kernel.elf"
install -m 0644 "$ROOT/iso/boot/grub/grub.cfg" "$STAGING/boot/grub/grub.cfg"

echo "[mkiso] Creating $ISO"
grub-mkrescue -o "$ISO" "$STAGING" --compress=no -V alpOS
if [[ ! -s "$ISO" ]]; then
  echo "ERROR: grub-mkrescue returned success but ISO is missing/empty." >&2
  exit 1
fi
echo "[mkiso] PASS: $(du -h "$ISO" | cut -f1) $ISO"
