#!/usr/bin/env python3
"""Apply the Limbo/VGA cursor and PS/2 keyboard initialization fix to alpOS."""
from pathlib import Path
import sys

p = Path(sys.argv[1] if len(sys.argv) > 1 else "kernel/arch/x86_64/main.c")
s = p.read_text()

old = '''static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1, %0":"=a"(v):"Nd"(p));return v;}\n'''
new = old + '''static void cursor_update(void){\n    uint16_t pos=(uint16_t)(cursor_y*VGA_WIDTH+cursor_x);\n    outb(0x3D4,0x0F);outb(0x3D5,(uint8_t)pos);\n    outb(0x3D4,0x0E);outb(0x3D5,(uint8_t)(pos>>8));\n}\nstatic void cursor_enable(void){\n    outb(0x3D4,0x0A);outb(0x3D5,0x0E);\n    outb(0x3D4,0x0B);outb(0x3D5,0x0F);\n    cursor_update();\n}\n'''
if old not in s:
    raise SystemExit('cannot find inb() insertion point; source does not match expected alpOS version')
s=s.replace(old,new,1)

old = '''if(cursor_y>=VGA_HEIGHT)scroll();}\n'''
new = '''if(cursor_y>=VGA_HEIGHT)scroll();cursor_update();}\n'''
if old not in s:
    raise SystemExit('cannot find console_putc() tail')
s=s.replace(old,new,1)

old = '''cursor_x=cursor_y=0;serial_putc(27);'''
new = '''cursor_x=cursor_y=0;cursor_update();serial_putc(27);'''
if old not in s:
    raise SystemExit('cannot find clear() cursor reset')
s=s.replace(old,new,1)

old = '''static void ps2_init(void){unsigned t;for(t=0;t<100000;t++)if(!(inb(0x64)&2))break;outb(0x64,0xAE);for(t=0;t<100000;t++)if(inb(0x64)&1)(void)inb(0x60);}\n'''
new = '''static int ps2_wait_input_empty(void){unsigned t;for(t=0;t<200000;t++)if(!(inb(0x64)&2))return 1;return 0;}\nstatic int ps2_wait_output_full(void){unsigned t;for(t=0;t<200000;t++)if(inb(0x64)&1)return 1;return 0;}\nstatic void ps2_write_data(uint8_t value){if(ps2_wait_input_empty())outb(0x60,value);}\nstatic void ps2_init(void){\n    uint8_t command_byte=0;\n    unsigned t;\n    /* Flush stale controller output before enabling the first PS/2 port. */\n    for(t=0;t<32&&(inb(0x64)&1);t++)(void)inb(0x60);\n    if(!ps2_wait_input_empty())return;\n    outb(0x64,0xAE); /* enable first PS/2 port */\n    if(ps2_wait_input_empty()){\n        outb(0x64,0x20); /* read controller configuration byte */\n        if(ps2_wait_output_full())command_byte=inb(0x60);\n    }\n    /* Enable set-1 translation, keep IRQs disabled (the kernel polls port 0x60). */\n    command_byte=(uint8_t)((command_byte|0x40u)&~0x11u);\n    if(ps2_wait_input_empty()){outb(0x64,0x60);ps2_write_data(command_byte);}\n    /* Explicitly enable keyboard scanning; tolerate controllers without ACK. */\n    ps2_write_data(0xF4);\n    if(ps2_wait_output_full())(void)inb(0x60);\n}\n'''
if old not in s:
    raise SystemExit('cannot find ps2_init()')
s=s.replace(old,new,1)

old = '''serial_init();ps2_init();if(magic!=0x36D76289)'''
new = '''serial_init();ps2_init();cursor_enable();if(magic!=0x36D76289)'''
if old not in s:
    raise SystemExit('cannot find kernel_main() initialization sequence')
s=s.replace(old,new,1)

p.write_text(s)
print(f'Updated {p}')
