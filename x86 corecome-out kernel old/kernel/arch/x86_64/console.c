/*
 * * console.c ---- VGA ???? + 16550A UART ??????
 * *
 * * ???????VGA ??????????UART ? QEMU ??????
 * * ??????????? putc?????????????
 * *
 * * ?????? paging_init ?????? C ?????????????
 * * ???? #PF?????? 0..4 MiB?? x86.h ???????
 * * start.S ????????console_init ??????????
 */
#include "x86.h"

#define VGA_COLS  80
#define VGA_ROWS  25
#define VGA_BASE  ((volatile uint16_t *)KERNEL_VMA + 0xB8000)

static uint16_t vga_x = 0, vga_y = 0;

void vga_init(void) { }

static void vga_scroll(void) {
    volatile uint16_t *m = VGA_BASE;
    for (int y = 1; y < VGA_ROWS; y++)
        for (int x = 0; x < VGA_COLS; x++)
            m[(y - 1) * VGA_COLS + x] = m[y * VGA_COLS + x];
    for (int x = 0; x < VGA_COLS; x++)
        m[(VGA_ROWS - 1) * VGA_COLS + x] = 0x0700;
}

static void vga_putchar(char c) {
    volatile uint16_t *m = VGA_BASE;
    if (c == '\n') {
        vga_x = 0; vga_y++;
    } else if (c == '\r') {
        vga_x = 0;
    } else if (c == '\b') {
        if (vga_x) vga_x--;
    } else {
        m[vga_y * VGA_COLS + vga_x] = (uint16_t)((0x07 << 8) | (uint8_t)c);
        vga_x++;
    }
    if (vga_x >= VGA_COLS) { vga_x = 0; vga_y++; }
    if (vga_y >= VGA_ROWS) { vga_scroll(); vga_y = VGA_ROWS - 1; }
}

void vga_putc(char c) { vga_putchar(c); }

/*
 * * serial_init ---- ? COM1 ???? 38400 8N1? FIFO ??
 * * ?????? 115200/38400=3DLAB=1 ??? DLL/DLM???? DLAB
 */
void serial_init(uint16_t port) {
    outb((uint16_t)(port + 1), 0x00);      /* IER = ??? */
    outb((uint16_t)(port + 3), 0x80);      /* LCR.DLAB = 1 */
    outb((uint16_t)(port + 0), 0x03);     /* DLL = 3  -> 38400 */
    outb((uint16_t)(port + 1), 0x00);     /* DLM = 0 */
    outb((uint16_t)(port + 3), 0x03);     /* 8N1, DLAB=0 */
    outb((uint16_t)(port + 2), 0xC7);     /* FIFO: 14 ?? + ?? */
    outb((uint16_t)(port + 4), 0x0B);     /* MCR: DTR|RTS|OUT2 */
    outb((uint16_t)(port + 1), 0x01);     /* IER: ?????? */
}

void serial_putc(uint16_t port, char c) {
 * LSR[5]=1 ?? THR ?????????????
    uint32_t n = 0xFFFFF;
    while ((inb((uint16_t)(port + 5)) & 0x20) == 0 && n--)
        io_wait();
    outb(port, (uint8_t)c);
}

/*
 * * console_putc ---- ??????early_putc ?????????????
 * * ???? UART?????????????????? port ?
 */
void console_putc(char c) {
    vga_putc(c);
    serial_putc(COM1, c);
}

void console_init(void) {
    serial_init(COM1);
    vga_init();
}
