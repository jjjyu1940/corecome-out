#include "alpos/syscall.h"
int main(void) {
    static const char name[] = "alpOS x86_64\n";
    return alpos_syscall3(ALPOS_SYS_WRITE, 1, (long)name, sizeof(name) - 1) < 0;
}
