#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 /path/to/corecome-out" >&2
  exit 2
fi
TARGET="$(cd "$1" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
cp -a "$HERE/README.md" "$TARGET/README.md"
cp -a "$HERE/kernel/.gitignore" "$TARGET/kernel/.gitignore"
cp -a "$HERE/kernel/Makefile.x86" "$TARGET/kernel/Makefile.x86"
mkdir -p "$TARGET/kernel/arch/x86_64/boot" "$TARGET/iso/boot/grub" "$TARGET/tools/image" "$TARGET/tools/ci" "$TARGET/.github/workflows"
cp -a "$HERE/kernel/arch/x86_64/boot/start.S" "$TARGET/kernel/arch/x86_64/boot/start.S"
cp -a "$HERE/kernel/arch/x86_64/boot/link.ld" "$TARGET/kernel/arch/x86_64/boot/link.ld"
cp -a "$HERE/kernel/arch/x86_64/main.c" "$TARGET/kernel/arch/x86_64/main.c"
cp -a "$HERE/iso/boot/grub/grub.cfg" "$TARGET/iso/boot/grub/grub.cfg"
cp -a "$HERE/tools/image/mkiso.sh" "$TARGET/tools/image/mkiso.sh"
cp -a "$HERE/tools/ci/verify_iso.py" "$TARGET/tools/ci/verify_iso.py"
cp -a "$HERE/tools/ci/verify_multiboot.py" "$TARGET/tools/ci/verify_multiboot.py"
cp -a "$HERE/.github/workflows/alpos-x86-iso.yml" "$TARGET/.github/workflows/alpos-x86-iso.yml"
echo "alpOS x86_64 stage-1 files copied into: $TARGET"
echo "Review the changes, then run: make -C kernel -f Makefile.x86 x86-test"
