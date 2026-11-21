/*
 * exec.c —— exec 系列 + 进程环境。
 *
 * 内核只提供 exec-by-path（SYS_EXEC_PATH = 19），吃一个路径和一个参数字符串。
 * 我们在这里把 argv[] 拼成 "argv[0]\0argv[1]\0..." 的单块，再调用它。
 * envp 当前不传（内核侧未实现 environ 语义），所以 execve 只能尽力而为。
 */

#include "libc.h"

/* 全局环境：当前内核未实现 environ 传递，始终为空。 */
char **environ = (char **)0;

int execve(const char *path, char *const argv[], char *const envp[])
{
    (void)envp;
    /*
     * 预算：每个参数字符串 + 一个空格/结尾，总长乘以 2 兜底。
     * 实际上每个 argv 元素占 "元素\0"，最大为 sum(len)+argc；
     * 这里给 4096 上限，超出直接 ENOMEM。
     */
    size_t total = 0;
    for (int i = 0; argv[i]; i++) total += strlen(argv[i]) + 1;
    if (total == 0 || total > 4096) { errno = E2BIG; return -1; }

    char *buf = (char *)sys_mmap((void *)0, total, 3 /* RW */, 0x20, -1, 0);
    if (buf == (void *)-1) { errno = ENOMEM; return -1; }

    size_t off = 0;
    for (int i = 0; argv[i]; i++) {
        size_t l = strlen(argv[i]);
        memcpy(buf + off, argv[i], l);
        off += l;
        buf[off++] = '\0';
    }
    sys_exec(path, buf);
    /* exec 成功不返回；走到这里就是失败 */
    sys_munmap(buf, total);
    errno = ENOENT;
    return -1;
}

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, (char *const *)0);
}

int execl(const char *path, const char *arg0, ...)
{
    /* 把可变参数收进一个固定大小的 argv 数组。参数个数上限 16。 */
    const char *argv[17];
    const char **ap = (const char **)&arg0;
    int n = 0;
    argv[n++] = arg0;
    while (*ap && n < 16) {
        argv[n] = *ap;
        if (argv[n] == (const char *)0) break;
        n++;
        ap++;
    }
    argv[n] = (const char *)0;
    return execv(path, (char *const *)argv);
}
