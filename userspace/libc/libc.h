/*
 * libc.h —— COXVBN 用户态最小 C 库。
 *
 * 只服务自己编译的静态 EL0 程序，不兼容 glibc/ musl ABI。
 * 所有 I/O 最终都过 svc #0x80；内核那一侧吃的是 kern.h 里的同一张号表。
 *
 * 调用约定（与内核 syscall.c 头注释保持一致）：
 *   svc #0x80 ；x8 = 号；x0..x2 = 参数；x0 = 返回值。
 *   负值返回一律当错误（最高位置 1）。
 */

#ifndef COXVBN_LIBC_H
#define COXVBN_LIBC_H

typedef unsigned long size_t;
typedef long          ssize_t;
typedef long          off_t;
typedef int           pid_t;
typedef unsigned int  uid_t;
typedef unsigned int  gid_t;
typedef unsigned int  mode_t;

/* ---- 系统调用号：必须和 kernel/include/kern.h / syscall/syscall.c 严格同步 ---- */
#define SYS_PUTS     1
#define SYS_TICKS    2
#define SYS_HEAP     3
#define SYS_WRITE    4
#define SYS_OPEN     5
#define SYS_READ     6
#define SYS_CLOSE    7
#define SYS_YIELD    8
#define SYS_EXIT     9
#define SYS_LIST    10
#define SYS_FORK    11
#define SYS_EXEC    12
#define SYS_WAIT    13
#define SYS_PID     14
#define SYS_PPID    15
#define SYS_CLONE   16
#define SYS_GETTID  17
#define SYS_KILL    18
#define SYS_EXEC_PATH 19
#define SYS_MKDIR   20
#define SYS_UNLINK  21
#define SYS_STAT    22
#define SYS_TIME    23
#define SYS_SLEEP   24
#define SYS_SEEK    25
#define SYS_RENAME  26
#define SYS_DUP     27
#define SYS_PIPE    28
#define SYS_ACCESS  29
#define SYS_SIGNAL  31
#define SYS_SIGRETURN 32
/* 类 Unix 新增：33..48。一旦加进内核，用户态这里要同步放开。 */
#define SYS_CHDIR    33
#define SYS_GETCWD   34
#define SYS_UMASK    35
#define SYS_GETUID   36
#define SYS_GETGID   37
#define SYS_SIGACTION 38
#define SYS_SIGPROCMASK 39
#define SYS_MMAP     40
#define SYS_MUNMAP   41
#define SYS_DUP2     42
#define SYS_FCNTL    43
#define SYS_FSTAT    44
#define SYS_GETPGID  45
#define SYS_SETPGID  46
#define SYS_IOCTL    47
#define SYS_MOUNT    48

/* ---- open flags（与内核 vfs_open 的位约定一致）---- */
#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_CREAT    1
#define O_TRUNC    2
#define O_APPEND   4

/* ---- 标准 fd ---- */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ---- errno：单线程，一个全局够用 ---- */
extern int errno;
#define	ENOENT   2   /* No such file or directory */
#define	EACCES  13   /* Permission denied */
#define	EFAULT  14   /* Bad address */
#define	EBADF   9    /* Bad file descriptor */
#define	EEXIST  17   /* File exists */
#define	EINVAL   22  /* Invalid argument */
#define	ENOMEM   12  /* Out of memory */
#define	ENOSYS   38  /* Function not implemented */
#define	EPIPE    32  /* Broken pipe */
#define	ECHILD   10  /* No child processes */
#define	ESRCH     3  /* No such process */

/* ---- 信号（与内核 sched 侧一致）---- */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV  11
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22
#define SIGUSR1  30
#define SIGUSR2  31

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int))-1)

#define SA_NOCLDSTOP 0x0001
#define SA_NOCLDWAIT 0x0002
#define SA_RESTART   0x1000

typedef struct {
    void  (*sa_handler)(int);
    unsigned long sa_flags;
    unsigned long sa_mask;
} struct sigaction;

/* ---- 文件状态（fstat 用，最小字段集）---- */
typedef struct {
    unsigned long st_dev;
    unsigned long st_ino;
    mode_t        st_mode;
    unsigned long st_nlink;
    uid_t         st_uid;
    gid_t         st_gid;
    unsigned long st_rdev;
    off_t         st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
} struct stat;

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_IRWXU  00700
#define S_IRUSR  00400
#define S_IWUSR  00200
#define S_IXUSR  00100
#define S_IRWXG  00070
#define S_IRGRP  00040
#define S_IWGRP  00020
#define S_IXGRP  00010
#define S_IRWXO  00007
#define S_IROTH  00004
#define S_IWOTH  00002
#define S_IXOTH  00001

/* ==================== 系统调用包装（汇编实现）==================== */
int          sys_puts(const char *s);
long         sys_ticks(void);
long         sys_write(int fd, const void *buf, size_t n);
long         sys_read(int fd, void *buf, size_t n);
int          sys_open(const char *path, int flags);
int          sys_close(int fd);
int          sys_dup(int fd);
int          sys_dup2(int oldfd, int newfd);
int          sys_fcntl(int fd, int cmd, long arg);
long         sys_seek(int fd, off_t off);
int          sys_fstat(int fd, struct stat *st);
int          sys_pipe(int pipefd[2]);
int          sys_mkdir(const char *path);
int          sys_unlink(const char *path);
int          sys_rename(const char *oldp, const char *newp);
long         sys_access(const char *path);
long         sys_stat(const char *path);
int          sys_chdir(const char *path);
int          sys_getcwd(char *buf, size_t size);
int          sys_umask(int mask);
uid_t        sys_getuid(void);
uid_t        sys_getgid(void);
int          sys_mount(const char *src, const char *tgt, const char *fstype);
int          sys_fork(void);
int          sys_exec(const char *path, const char *arg);
int          sys_wait(int pid);
int          sys_getpid(void);
int          sys_getppid(void);
int          sys_gettid(void);
int          sys_kill(pid_t pid, int sig);
int          sys_exit(int code) __attribute__((noreturn));
int          sys_sigaction(int sig, const struct sigaction *act,
                          struct sigaction *oldact);
int          sys_sigprocmask(int how, unsigned long *set, unsigned long *oldset);
int          sys_ioctl(int fd, unsigned long req, unsigned long arg);
void        *sys_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int          sys_munmap(void *addr, size_t len);
int          sys_getpgid(pid_t pid);
int          sys_setpgid(pid_t pid, pid_t pgid);

/* ==================== 标准 C 库（最小子集）==================== */
void         _start(void) __attribute__((noreturn));
void         exit(int status) __attribute__((noreturn));

int          putchar(int c);
int          puts(const char *s);
int          printf(const char *fmt, ...);
int          write(int fd, const void *buf, size_t n);
ssize_t      read(int fd, void *buf, size_t n);
int          open(const char *path, int flags, ...);
int          close(int fd);
int          dup(int fd);
int          dup2(int oldfd, int newfd);
int          pipe(int pipefd[2]);
off_t        lseek(int fd, off_t offset, int whence);
int          unlink(const char *path);
int          mkdir(const char *path, mode_t mode);
int          rmdir(const char *path);
int          rename(const char *oldp, const char *newp);
int          chdir(const char *path);
char        *getcwd(char *buf, size_t size);
mode_t       umask(mode_t mask);
int          access(const char *path, int amode);
int          fstat(int fd, struct stat *st);
int          stat(const char *path, struct stat *st);
int          kill(pid_t pid, int sig);
int          fork(void);
int          execve(const char *path, char *const argv[], char *const envp[]);
pid_t        getpid(void);
pid_t        getppid(void);
pid_t        gettid(void);
uid_t        getuid(void);
gid_t        getgid(void);
int          getpgid(pid_t pid);
int          setpgid(pid_t pid, pid_t pgid);
void        *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
int          munmap(void *addr, size_t length);
int          ioctl(int fd, unsigned long request, ...);
int          sigaction(int sig, const struct sigaction *act,
                      struct sigaction *oldact);
int          sigprocmask(int how, const unsigned long *set,
                        unsigned long *oldset);
unsigned int sleep(unsigned int seconds);
unsigned int alarm(unsigned int seconds);
void         perror(const char *s);

/* 字符串 */
size_t       strlen(const char *s);
int          strcmp(const char *a, const char *b);
int          strncmp(const char *a, const char *b, size_t n);
char        *strcpy(char *dst, const char *src);
char        *strncpy(char *dst, const char *src, size_t n);
char        *strcat(char *dst, const char *src);
char        *strchr(const char *s, int c);
char        *strrchr(const char *s, int c);
void        *memset(void *s, int c, size_t n);
void        *memcpy(void *dst, const void *src, size_t n);
int          memcmp(const void *a, void *b, size_t n);
char        *strdup(const char *s);
char        *strtok_r(char *s, const char *delim, char **save);

/* 转换 */
int          atoi(const char *s);
long         strtol(const char *s, char **end, int base);
char        *itoa(int val, char *buf, int base);

/* 进程环境 */
extern char **environ;
int          execv(const char *path, char *const argv[]);
int          execl(const char *path, const char *arg0, ...);

#endif /* COXVBN_LIBC_H */
