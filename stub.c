#include <stdint.h>
void kernel_main(uint32_t magic, uint32_t mbi) { (void)magic; (void)mbi; for (;;) __asm__ volatile("hlt"); }
