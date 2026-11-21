/*
 * stdio.c —— 基于 write(2) 的最小 stdio 与 printf。
 *
 * 不做缓冲（EL0 里 /dev/console 是非阻塞轮询，缓冲反而让提示符卡住），
 * 每个 putchar 直接 svc。格式串只支持 %s %c %d %u %x %p %%，够跑 shell 就行。
 */

#include "libc.h"

/* errno 是线程局部的弱符号；当前内核单线程，一个全局足够。 */
int errno;

int putchar(int c)
{
    char b = (char)c;
    if (sys_write(STDOUT_FILENO, &b, 1) != 1) return EOF;
    return c;
}

int puts(const char *s)
{
    if (sys_write(STDOUT_FILENO, s, strlen(s)) < 0) return EOF;
    putchar('\n');
    return 0;
}

/* 把 n 位十进制整数（>=0）写成文本，返回写入字符数。 */
static int put_uint(unsigned long v, int base, int is_upper)
{
    char buf[24];
    int i = 0;
    int n = 0;
    char a = is_upper ? 'A' : 'a';
    if (v == 0) {
        putchar('0');
        return 1;
    }
    while (v) {
        unsigned long d = v % base;
        buf[i++] = (char)(d < 10 ? '0' + d : a + (d - 10));
        v /= base;
    }
    while (i--) { putchar(buf[i]); n++; }
    return n;
}

/* printf：不支持浮点、不支持宽度/精度修饰。 */
int printf(const char *fmt, ...)
{
    unsigned long *ap = (unsigned long *)((char *)&fmt + sizeof(fmt) + 15);
    int n = 0;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            putchar(*fmt); n++;
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = (const char *)*ap++;
            if (!s) s = "(null)";
            sys_write(STDOUT_FILENO, s, strlen(s));
            n += (int)strlen(s);
            break;
        }
        case 'c':
            putchar((int)*ap++); n++;
            break;
        case 'd':
        case 'i': {
            long v = (long)*ap++;
            if (v < 0) { putchar('-'); v = -v; n++; }
            n += put_uint((unsigned long)v, 10, 0);
            break;
        }
        case 'u':
            n += put_uint((unsigned long)*ap++, 10, 0);
            break;
        case 'x':
            n += put_uint((unsigned long)*ap++, 16, 0);
            break;
        case 'X':
            n += put_uint((unsigned long)*ap++, 16, 1);
            break;
        case 'p': {
            putchar('0'); putchar('x');
            n += 2 + put_uint((unsigned long)*ap++, 16, 0);
            break;
        }
        case '%':
            putchar('%'); n++;
            break;
        case 'l':
            /* %ld / %lu：忽略 l，按 64 位处理（我们的 long 本来就是 64 位） */
            fmt++;
            if (*fmt == 'd' || *fmt == 'i') {
                long v = (long)*ap++;
                if (v < 0) { putchar('-'); v = -v; n++; }
                n += put_uint((unsigned long)v, 10, 0);
            } else if (*fmt == 'u') {
                n += put_uint((unsigned long)*ap++, 10, 0);
            } else if (*fmt == 'x') {
                n += put_uint((unsigned long)*ap++, 16, 0);
            }
            break;
        default:
            putchar('%'); putchar(*fmt); n += 2;
            break;
        }
    }
    return n;
}

/* ---- 对外的文件操作包装：自动 varargs 转 syscall ---- */

int write(int fd, const void *buf, size_t n)
{
    return (int)sys_write(fd, buf, n);
}

ssize_t read(int fd, void *buf, size_t n)
{
    return (ssize_t)sys_read(fd, buf, n);
}

int open(const char *path, int flags, ...)
{
    return sys_open(path, flags);
}

int close(int fd)
{
    return sys_close(fd);
}

int dup(int fd)
{
    return sys_dup(fd);
}

int dup2(int oldfd, int newfd)
{
    return sys_dup2(oldfd, newfd);
}

int pipe(int pipefd[2])
{
    return sys_pipe(pipefd);
}

off_t lseek(int fd, off_t offset, int whence)
{
    off_t cur;
    switch (whence) {
    case SEEK_SET: return (off_t)sys_seek(fd, offset);
    case SEEK_CUR: cur = (off_t)sys_seek(fd, 0); return (off_t)sys_seek(fd, cur + offset);
    case SEEK_END: {
        /* stat 拿大小再 seek：这里用 sys_stat，文件名需额外传，故不支持 */
        return (off_t)sys_seek(fd, offset);
    }
    default: errno = EINVAL; return -1;
    }
}

int unlink(const char *path)
{
    return sys_unlink(path);
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return sys_mkdir(path);
}

int rmdir(const char *path)
{
    return sys_unlink(path); /* ramfs 里目录也是 unlink */
}

int rename(const char *oldp, const char *newp)
{
    return sys_rename(oldp, newp);
}

int chdir(const char *path)
{
    return sys_chdir(path);
}

char *getcwd(char *buf, size_t size)
{
    if (sys_getcwd(buf, size) < 0) return (char *)0;
    return buf;
}

mode_t umask(mode_t mask)
{
    return (mode_t)sys_umask(mask & 0777);
}

int access(const char *path, int amode)
{
    (void)amode;
    return sys_access(path);
}

int fstat(int fd, struct stat *st)
{
    return sys_fstat(fd, st);
}

int stat(const char *path, struct stat *st)
{
    long sz = sys_stat(path);
    if (sz < 0) return -1;
    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)sz;
    st->st_mode = S_IFREG | 0644;
    return 0;
}

int kill(pid_t pid, int sig)
{
    return sys_kill(pid, sig);
}

int fork(void)
{
    return sys_fork();
}

pid_t getpid(void) { return (pid_t)sys_getpid(); }
pid_t getppid(void) { return (pid_t)sys_getppid(); }
pid_t gettid(void) { return (pid_t)sys_gettid(); }
uid_t getuid(void) { return sys_getuid(); }
gid_t getgid(void) { return sys_getgid(); }

int getpgid(pid_t pid) { return sys_getpgid(pid); }
int setpgid(pid_t pid, pid_t pgid) { return sys_setpgid(pid, pgid); }

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return sys_mmap(addr, length, prot, flags, fd, offset);
}

int munmap(void *addr, size_t length)
{
    return sys_munmap(addr, length);
}

int ioctl(int fd, unsigned long request, ...)
{
    unsigned long *ap = (unsigned long *)&request + 1;
    return sys_ioctl(fd, request, *ap);
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    return sys_sigaction(sig, act, oldact);
}

int sigprocmask(int how, const unsigned long *set, unsigned long *oldset)
{
    (void)how;
    return sys_sigprocmask((unsigned long)set, (unsigned long)oldset);
}

unsigned int sleep(unsigned int seconds)
{
    sys_sleep(seconds * 100); /* 内核 SYS_SLEEP 单位是 tick（100 Hz） */
    return 0;
}

unsigned int alarm(unsigned int seconds)
{
    (void)seconds;
    /* 内核未实现间隔定时器，留接口返回 0（未取消任何旧定时器） */
    return 0;
}

void perror(const char *s)
{
    static const char *errmsg[] = {
        [0] = "success",
        [ENOENT] = "No such file or directory",
        [EACCES] = "Permission denied",
        [EFAULT] = "Bad address",
        [EBADF] = "Bad file descriptor",
        [EEXIST] = "File exists",
        [EINVAL] = "Invalid argument",
        [ENOMEM] = "Out of memory",
        [ENOSYS] = "Function not implemented",
        [EPIPE] = "Broken pipe",
        [ECHILD] = "No child processes",
        [ESRCH] = "No such process",
    };
    if (s && *s) { sys_write(2, s, strlen(s)); sys_write(2, ": ", 2); }
    const char *m = (errno >= 0 && (size_t)errno < sizeof(errmsg)/sizeof(errmsg[0]) && errmsg[errno])
                    ? errmsg[errno] : "Unknown error";
    sys_write(2, m, strlen(m));
    sys_write(2, "\n", 1);
}
