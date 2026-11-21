CROSS ?= x86_64-linux-gnu-
CC := $(CROSS)gcc
AS := $(CROSS)gcc
LD := $(CROSS)ld
BUILD := build
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -m64 -mno-red-zone -mgeneral-regs-only -mno-sse -mno-mmx -Iinclude
LDFLAGS := -m elf_x86_64 -T link.ld -nostdlib
PROGRAMS := echo uname
.PHONY: all clean verify initramfs
all: $(PROGRAMS:%=$(BUILD)/%)
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: lib/start.S | $(BUILD)
	$(AS) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: bin/%.c include/alpos/syscall.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%: $(BUILD)/start.o $(BUILD)/%.o link.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/start.o $(BUILD)/$*.o
verify: all
	@for p in $(PROGRAMS); do readelf -h $(BUILD)/$$p | grep -q 'ELF64'; readelf -h $(BUILD)/$$p | grep -q 'EXEC'; done
	@echo '[userland] ELF64 executable checks passed'
initramfs: verify
	python3 ../tools/mknewc.py $(BUILD)/initramfs.cpio bin/echo=$(BUILD)/echo bin/uname=$(BUILD)/uname
clean:
	rm -rf $(BUILD)
