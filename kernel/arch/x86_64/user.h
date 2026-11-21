#ifndef ALPOS_USER_H
#define ALPOS_USER_H
#include <stdint.h>
void user_init(uint32_t mbi_phys);
int user_exec(const char *name, const char *args);
long syscall_dispatch(void *frame);
extern volatile int user_exit_status;
#endif
