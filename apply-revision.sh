#!/usr/bin/env bash
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="${1:-.}"
DST="$(cd "$DST" && pwd)"
for f in README.md kernel/arch/x86_64/main.c kernel/arch/x86_64/gui/gfx.c kernel/arch/x86_64/gui/gfx.h kernel/Makefile.x86 .github/workflows/alpos-build.yml; do
  mkdir -p "$DST/$(dirname "$f")"
  cp "$SRC/$f" "$DST/$f"
done
echo "Applied alpOS bilingual README, full-screen terminal, slash rendering/key mapping, and GitHub Actions workflow."
echo "Review git diff, then run: make clean && make iso && make test"
