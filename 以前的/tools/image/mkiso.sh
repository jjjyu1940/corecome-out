#!/usr/bin/env bash
#
# mkiso.sh —— 把 x86_64 内核 ELF 打成可启动 ISO。
#
# 依赖（均来自 gcc-multilib + grub-pc-bin + xorriso，CI 镜像已装）：
#   x86_64-linux-gnu-gcc       编译内核 C
#   x86_64-linux-gnu-ld        链接 ELF
#   grub-mkrescue              产出 hybrid ISO（BIOS + UEFI 双启动）
#   xorriso                    grub-mkrescue 的实际后端
#
# 流程：
#   1. 编译 kernel/arch/x86_64/** 下的全部 .c/.S；
#   2. 用 arch/x86_64/boot/link.ld 链接成 build/alpos-kernel.elf；
#   3. 把 ELF 放进 iso/boot/，补 grub.cfg，跑 grub-mkrescue；
#   4. 产出 build/alpos-<git短哈希>.iso，同时更新 build/alpos-latest.iso。
#
# 失败处理：任何一步非零退出立即终止并保留 build/ 现场，方便定位。

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KERNEL="$ROOT/kernel"
BUILD="$KERNEL/build"
ISO_ROOT="$ROOT/iso"
OUT="$BUILD/alpos-latest.iso"

: "${CROSS:=x86_64-linux-gnu-}"
CC="${CROSS}gcc"
LD="${CROSS}ld"

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[mkiso] cross compiler $CC not found; try apt-get install gcc-x86-64-linux-gnu" >&2
    exit 127
fi

mkdir -p "$BUILD"

SRCS=(
    arch/x86_64/boot/start.S
    arch/x86_64/context/gdt.S
    arch/x86_64/exception/isr.S
    arch/x86_64/main.c
    arch/x86_64/console.c
    arch/x86_64/gdt.c
    arch/x86_64/idt.c
    arch/x86_64/pic.c
    arch/x86_64/panic.c
    arch/x86_64/usermode.c
    arch/x86_64/syscall.c
    arch/x86_64/mmu/paging.c
    arch/x86_64/init.c
)

objs_of() {
    local s="$1"
    local base
    base="$(basename "$s")"
    case "$base" in
        *.S) base="${base%.S}" ;;
        *.c) base="${base%.c}" ;;
        *)   base="${base%.*}" ;;
    esac
    echo "$BUILD/${base}.o"
}

CFLAGS=(
    -m64 -mno-red-zone -mgeneral-regs-only -mno-sse -mno-mmx
    -ffreestanding -fno-pic -fno-pie -fno-stack-protector
    -fno-builtin -nostdlib
    -O2 -Wall -Wextra -Werror -g
    -Iarch/x86_64/include -Iinclude
)

echo "[mkiso] compiling x86_64 objects..."
OBJS=()
for s in "${SRCS[@]}"; do
    o="$(objs_of "$s")"
    OBJS+=("$o")
    echo "  CC  $s"
    "$CC" "${CFLAGS[@]}" -c "$KERNEL/$s" -o "$o" \
        || { echo "[mkiso] compile failed: $s" >&2; exit 1; }
done

echo "[mkiso] linking ELF..."
"$LD" -T arch/x86_64/boot/link.ld -nostdlib -z max-page-size=4096 \
      -o "$BUILD/alpos-kernel.elf" \
      "${OBJS[@]}" \
    || { echo "[mkiso] link failed" >&2; exit 1; }

mkdir -p "$ISO_ROOT/boot"
cp "$BUILD/alpos-kernel.elf" "$ISO_ROOT/boot/"

if ! command -v grub-mkrescue >/dev/null 2>&1; then
    echo "[mkiso] grub-mkrescue missing; building ISOLINUX fallback" >&2
    echo "[mkiso] ISOLINUX fallback not implemented in M0; install grub-mkrescue" >&2
    exit 127
else
    echo "[mkiso] building hybrid ISO (BIOS + UEFI)..."
    grub-mkrescue -o "$OUT" "$ISO_ROOT" --compress=no \
        -V "alpOS" \
      || { echo "[mkiso] grub-mkrescue failed" >&2; exit 1; }
fi

SHA=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo "dev")
mv "$OUT" "$BUILD/alpos-${SHA}.iso"
cp "$BUILD/alpos-${SHA}.iso" "$OUT"

ls -lh "$BUILD/alpos-${SHA}.iso"
echo "[mkiso] done: $BUILD/alpos-${SHA}.iso"
