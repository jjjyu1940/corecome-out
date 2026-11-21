#include <stddef.h>
#include <stdint.h>

#define COM1 0x3F8
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static size_t cursor_x;
static size_t cursor_y;
static const uint8_t color = 0x07;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01); /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8N1 */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    unsigned int timeout = 1000000;
    while ((inb(COM1 + 5) & 0x20) == 0 && --timeout) { }
    if (timeout)
        outb(COM1, (uint8_t)c);
}

static void vga_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; ++y)
        for (size_t x = 0; x < VGA_WIDTH; ++x)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; ++x)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)(color << 8) | ' ';
    cursor_y = VGA_HEIGHT - 1;
}

static void console_putc(char c)
{
    serial_putc(c);
    if (c == '\n') {
        cursor_x = 0;
        ++cursor_y;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x)
            --cursor_x;
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)(color << 8) | ' ';
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)(color << 8) | (uint8_t)c;
        if (++cursor_x == VGA_WIDTH) {
            cursor_x = 0;
            ++cursor_y;
        }
    }
    if (cursor_y >= VGA_HEIGHT)
        vga_scroll();
}

static void puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void clear_screen(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
        VGA_MEMORY[i] = (uint16_t)(color << 8) | ' ';
    cursor_x = 0;
    cursor_y = 0;
    serial_putc(27);
    serial_putc('[');
    serial_putc('2');
    serial_putc('J');
    serial_putc(27);
    serial_putc('[');
    serial_putc('H');
}

static char serial_getc(void)
{
    while ((inb(COM1 + 5) & 1) == 0)
        __asm__ volatile ("pause");
    return (char)inb(COM1);
}

static void prompt(void)
{
    puts("alpos> ");
}

static void run_command(const char *line, uint32_t magic, uint32_t mbi)
{
    if (streq(line, "help")) {
        puts("Commands: help, about, clear, mem, reboot, halt\n");
    } else if (streq(line, "about")) {
        puts("alpOS x86_64 - minimal bootable kernel\n");
        puts("Multiboot2 boot completed; interactive serial shell is ready.\n");
    } else if (streq(line, "clear")) {
        clear_screen();
    } else if (streq(line, "mem")) {
        puts("Boot protocol: Multiboot2\n");
        puts("Boot info address: 0x");
        for (int shift = 28; shift >= 0; shift -= 4) {
            const char hex[] = "0123456789ABCDEF";
            console_putc(hex[(mbi >> shift) & 0xF]);
        }
        puts("\nBoot magic: 0x");
        for (int shift = 28; shift >= 0; shift -= 4) {
            const char hex[] = "0123456789ABCDEF";
            console_putc(hex[(magic >> shift) & 0xF]);
        }
        puts("\n");
    } else if (streq(line, "reboot")) {
        puts("Rebooting...\n");
        while (inb(0x64) & 0x02) { }
        outb(0x64, 0xFE);
    } else if (streq(line, "halt")) {
        puts("System halted.\n");
        for (;;) __asm__ volatile ("cli; hlt");
    } else if (*line) {
        puts("Unknown command. Type 'help'.\n");
    }
}

void kernel_main(uint32_t magic, uint32_t mbi)
{
    serial_init();
    if (magic != 0x36D76289) {
        puts("[alpOS] ERROR: invalid Multiboot2 magic\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    clear_screen();
    puts("========================================\n");
    puts("              alpOS x86_64              \n");
    puts("========================================\n");
    puts("Booted via GRUB / Multiboot2.\n");
    puts("Type 'help' to list available commands.\n");
    puts("[alpOS] shell ready\n\n");

    char line[96];
    for (;;) {
        size_t length = 0;
        prompt();
        for (;;) {
            char c = serial_getc();
            if (c == '\r' || c == '\n') {
                console_putc('\n');
                line[length] = '\0';
                break;
            }
            if ((c == '\b' || c == 0x7F) && length) {
                --length;
                console_putc('\b');
            } else if (c >= 0x20 && c <= 0x7E && length + 1 < sizeof(line)) {
                line[length++] = c;
                console_putc(c);
            }
        }
        run_command(line, magic, mbi);
    }
}
