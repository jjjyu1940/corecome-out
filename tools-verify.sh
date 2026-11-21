#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
mkdir -p build-check
x86_64-linux-gnu-gcc -m64 -ffreestanding -fno-pic -fno-pie -c \
  kernel/arch/x86_64/boot/start.S -o build-check/start.o
cat > build-check/stub.c <<'SRC'
#include <stdint.h>
void kernel_main(uint32_t magic, uint32_t mbi) {
    (void)magic; (void)mbi;
    for (;;) __asm__ volatile("hlt");
}
SRC
x86_64-linux-gnu-gcc -m64 -mcmodel=small -mno-red-zone -mgeneral-regs-only \
  -mno-sse -mno-mmx -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
  -fno-builtin -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib \
  -O2 -Wall -Wextra -Werror -c build-check/stub.c -o build-check/stub.o
x86_64-linux-gnu-ld -T kernel/arch/x86_64/boot/link.ld -nostdlib \
  -z max-page-size=0x1000 -o build-check/test.elf \
  build-check/start.o build-check/stub.o
readelf -h build-check/test.elf | grep -q 'Advanced Micro Devices X86-64'
python3 - build-check/test.elf <<'PY'
import struct, sys
blob = open(sys.argv[1], 'rb').read()
off = blob.find(struct.pack('<I', 0xE85250D6))
assert 0 <= off < 32768, f'Multiboot2 header offset invalid: {off}'
magic, arch, length, checksum = struct.unpack_from('<4I', blob, off)
assert magic == 0xE85250D6 and arch == 0 and length >= 24
assert (magic + arch + length + checksum) & 0xffffffff == 0
print(f'PASS: Multiboot2 header at {off}, checksum valid')
PY
