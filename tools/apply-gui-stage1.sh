#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-.}"
mkdir -p "$DEST/kernel/arch/x86_64/gui" "$DEST/kernel/arch/x86_64/boot" "$DEST/tools/ci" "$DEST/.github/workflows"
cp "$ROOT/kernel/arch/x86_64/main.c" "$DEST/kernel/arch/x86_64/main.c"
cp "$ROOT/kernel/arch/x86_64/boot/start.S" "$DEST/kernel/arch/x86_64/boot/start.S"
cp "$ROOT/kernel/Makefile.x86" "$DEST/kernel/Makefile.x86"
cp "$ROOT/kernel/arch/x86_64/gui/gfx.c" "$DEST/kernel/arch/x86_64/gui/gfx.c"
cp "$ROOT/kernel/arch/x86_64/gui/gfx.h" "$DEST/kernel/arch/x86_64/gui/gfx.h"
cp "$ROOT/tools/ci/verify_gui_header.py" "$DEST/tools/ci/verify_gui_header.py"
cp "$ROOT/.github/workflows/alpos-build.yml" "$DEST/.github/workflows/alpos-build.yml"
echo "[apply-gui-stage1] overlay copied into $DEST"
