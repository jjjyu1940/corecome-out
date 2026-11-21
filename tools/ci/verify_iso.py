#!/usr/bin/env python3
"""
verify_iso.py —— ISO 产出物的静态校验。

不做启动测试，只确认三件事：
  1. 文件确实是 El Torito 引导映像（有启动目录 + 至少一个 boot entry）；
  2. 根目录里存在 /boot/grub/grub.cfg 与 /boot/alpos-kernel.elf；
  3. 内核 ELF 带有 Multiboot2 魔数（0x1BADB002 / 0xE85250D6），否则
     GRUB 会在加载阶段拒绝，报“no multiboot header found”。

退出码：0 校验通过；1 文件不存在或格式错；2 校验项缺失。
依赖：xorriso（用于 el-torito 检查）与 python3 标准库。
"""
import sys
import os
import struct
import subprocess

MULTIBOOT1 = 0x1BADB002
MULTIBOOT2 = 0xE85250D6


def fail(code, msg):
    print(f"[verify_iso] FAIL ({code}): {msg}", file=sys.stderr)
    sys.exit(code)


def check_elf(path):
    with open(path, "rb") as f:
        head = f.read(64)
    if len(head) < 52:
        fail(2, f"{path}: too small to be an ELF")
    if head[:4] != b"\x7fELF":
        fail(2, f"{path}: not an ELF file")
    ei_class = head[4]
    ei_data = head[5]
    if ei_class != 2:
        fail(2, f"{path}: not ELF64 (class={ei_class})")
    if ei_data != 1 and ei_data != 2:
        fail(2, f"{path}: bad ELF data encoding")
    e_type, e_machine, e_version = struct.unpack_from("<HHI", head, 16)
    if e_machine not in (62, 0x3E):          # EM_X86_64
        fail(2, f"{path}: not x86_64 ELF (machine={e_machine:#x})")
    print(f"[verify_iso] ELF OK: type={e_type} machine={e_machine:#x}")


def has_multiboot(path):
    """扫描 ELF 的全部 PT_LOAD 段，查找魔数；不依赖 readelf 存在。"""
    with open(path, "rb") as f:
        head = f.read(64)
    ei_data = head[5]
    endian = "<" if ei_data == 1 else ">"
    e_phoff = struct.unpack_from(endian + "Q", head, 24)[0]
    e_phentsize, e_phnum = struct.unpack_from(endian + "HH", head, 54)[0:2]
    with open(path, "rb") as f:
        f.seek(e_phoff)
        phdr = f.read(e_phentsize * e_phnum)
    found = []
    for i in range(e_phnum):
        p = phdr[i * e_phentsize:(i + 1) * e_phentsize]
        p_type, p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from(
            endian + "IQQQQ", p, 0)
        if p_filesz == 0:
            continue
        with open(path, "rb") as f:
            f.seek(p_offset)
            data = f.read(min(p_filesz, 1024 * 1024))
        for m in (MULTIBOOT1, MULTIBOOT2):
            off = 0
            while True:
                idx = data.find(struct.pack(endian + "I", m), off)
                if idx == -1:
                    break
                found.append((m, p_offset + idx))
                off = idx + 1
    return found


def check_multiboot(kernel):
    hits = has_multiboot(kernel)
    if not hits:
        fail(2, f"{kernel}: no Multiboot1/2 magic found; "
                  "GRUB will reject with 'no multiboot header found'")
    for m, off in hits:
        name = "MB1" if m == MULTIBOOT1 else "MB2"
        print(f"[verify_iso] Multiboot OK: {name} at file offset {off}")
    return True


def check_eltorito(iso):
    """用 xorriso 读 El Torito 启动目录，没启动目录说明无法从 CD/USB 起。"""
    r = subprocess.run(
        ["xorriso", "-indev", iso, "-report_el_torito", "plain", "as_mkisofs"],
        capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        fail(2, f"{iso}: xorriso report failed")
    text = r.stdout
    if "Boot record type" not in text:
        fail(2, f"{iso}: not a bootable El Torito image")
    boot_entries = [l for l in text.splitlines()
                    if "El Torito" in l and "boot entry" in l.lower()]
    if not boot_entries:
        fail(2, f"{iso}: no El Torito boot entries")
    print(f"[verify_iso] El Torito OK: {len(boot_entries)} boot entry/entries")


def check_iso_contents(iso, kernel_path):
    """在 ISO 里按路径找文件。"""
    r = subprocess.run(
        ["xorriso", "-indev", iso, "-find", "/"],
        capture_output=True, text=True, timeout=120)
    files = set(l.strip().strip("/") for l in r.stdout.splitlines())
    for required in ("boot/grub/grub.cfg", "boot/alpos-kernel.elf"):
        if required not in files:
            fail(2, f"{iso}: missing {required}")
    print(f"[verify_iso] contents OK: grub.cfg + kernel.elf present")


def main():
    if len(sys.argv) < 2:
        print("usage: verify_iso.py <alpos.iso> [kernel.elf]", file=sys.stderr)
        sys.exit(2)
    iso = sys.argv[1]
    kernel = sys.argv[2] if len(sys.argv) > 2 else None
    if not os.path.isfile(iso):
        fail(1, f"{iso}: not found")

    check_iso_contents(iso, kernel)
    check_eltorito(iso)
    if kernel and os.path.isfile(kernel):
        check_elf(kernel)
        check_multiboot(kernel)
    elif kernel:
        print(f"[verify_iso] WARN: kernel {kernel} not found, "
              "skipping multiboot check", file=sys.stderr)
    print("[verify_iso] all checks passed")
    sys.exit(0)


if __name__ == "__main__":
    main()
