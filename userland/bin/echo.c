#include <stddef.h>
#include "alpos/syscall.h"
int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; ++i) {
        size_t n = 0;
        while (argv[i][n]) ++n;
        if (alpos_syscall3(ALPOS_SYS_WRITE, 1, (long)argv[i], (long)n) < 0) return 1;
        if (i + 1 < argc) alpos_syscall3(ALPOS_SYS_WRITE, 1, (long)" ", 1);
    }
    alpos_syscall3(ALPOS_SYS_WRITE, 1, (long)"\n", 1);
    return 0;
}
