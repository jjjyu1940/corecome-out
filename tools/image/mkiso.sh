#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KERNEL="$ROOT/kernel"
BUILD="$KERNEL/build-x86"
STAGING="$BUILD/iso-root"
command -v grub-mkrescue >/dev/null || { echo 'missing grub-mkrescue' >&2; exit 127; }
command -v xorriso >/dev/null || { echo 'missing xorriso' >&2; exit 127; }
test -s "$BUILD/alpos-kernel.elf" || { echo 'kernel ELF missing; run make first' >&2; exit 1; }
rm -rf "$STAGING"; mkdir -p "$STAGING/boot/grub"
cp "$BUILD/alpos-kernel.elf" "$STAGING/boot/alpos-kernel.elf"
cp "$ROOT/iso/boot/grub/grub.cfg" "$STAGING/boot/grub/grub.cfg"
grub-mkrescue -o "$BUILD/alpos.iso" "$STAGING" --compress=no -V alpOS
