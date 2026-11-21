#include "alpos/syscall.h"
int main(void){static const char s[]="alpOS x86_64\n";return alpos_syscall3(ALPOS_SYS_WRITE,1,(long)s,sizeof(s)-1)<0;}
