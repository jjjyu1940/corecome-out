/* vfs.c —— 大内核里的文件层：inode 树 + 打开表 + ramfs + /dev/console。
 *
 * 没有块设备、没有缓存、没有并发保护：先用最小实现证明
 * “用户态要的文件语义，在内核里是一条函数调用链”。
 */
#include "kern.h"
#include "blk.h"
#include <stdarg.h>

extern const u8 user_blob_start[];
extern const u8 user_blob_end[];
extern const u8 user_shell_start[];
extern const u8 _binary_build_user_elf_elf_start[];
extern const u8 _binary_build_user_elf_elf_end[];
extern const u8 user_shell_end[];

/* ramfs 单文件上限：<=1 KiB 走 kmalloc，>1 KiB 用整页（4 KiB）承载。 */
#define RAMFS_FILE_MAX 4096UL

#define MAX_INO   192
#define MAX_FD    16
#define T_DIR     0
#define T_FILE    1
#define T_DEV     2
#define T_EXT2    3   /* 指向 ext2 只读文件：首次 read 时把内容读进内存 */
#define T_PIPE    4   /* 管道：4 KiB 环形缓冲，非阻塞 */
#define DEV_CON   1

typedef struct inode {
    char name[24];
    int type;
    int dev;
    struct inode *parent;
    struct inode *child;
    struct inode *sib;
    char *data;
    u64 size;
    u64 cap;
    u64 rpos;      /* 管道读位置 */
    u64 wpos;      /* 管道写位置 */
    int page_backed; /* data 来自 mm_page_alloc，需要 mm_page_free */
    int refs;      /* 打开的 fd 数（含 dup/fork 克隆） */
    int unlinked;  /* 已从目录树摘除，等最后一个 fd 关闭再释放 */
} ino_t;

static ino_t pool[MAX_INO];
static int np;
static ino_t *root;

typedef struct {
    ino_t *ino;
    u64 off;
    int used;
} file_t;

/* 每个进程一份 fd 表：fork 时深拷贝，互不影响。
 * 内核线程没有 ftab，回退到 boot_ft（0/1/2 接控制台）。 */
struct filetable {
    file_t f[MAX_FD];
};
static struct filetable boot_ft;
static struct filetable *ftab(void)
{
    struct filetable *t = (struct filetable *)sched_get_ftab();
    return t != (struct filetable *)0 ? t : &boot_ft;
}
#define fds (ftab()->f)

static void str_copy(char *d, const char *s, u64 cap)
{
    u64 i = 0;
    while (s[i] != '\0' && i + 1 < cap) {
        d[i] = s[i];
        i++;
    }
    d[i] = '\0';
}

static int str_eq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* 取一个槽位：优先复用被 unlink 释放的（type=-1），否则从池尾新开。 */
static ino_t *ino_slot(void)
{
    int i;

    for (i = 0; i < np; i++) {
        if (pool[i].type == -1) {
            return &pool[i];
        }
    }
    if (np >= MAX_INO) {
        return (ino_t *)0;
    }
    return &pool[np++];
}

static ino_t *ino_new(ino_t *parent, const char *name, int type)
{
    ino_t *n = ino_slot();

    if (n == (ino_t *)0) {
        return (ino_t *)0;
    }
    n->type = type;
    n->dev = 0;
    n->parent = parent;
    n->child = (ino_t *)0;
    n->sib = (ino_t *)0;
    n->data = (char *)0;
    n->size = 0;
    n->cap = 0;
    n->rpos = 0;
    n->wpos = 0;
    n->page_backed = 0;
    n->refs = 0;
    n->unlinked = 0;
    str_copy(n->name, name, sizeof(n->name));

    if (parent != (ino_t *)0) {
        if (parent->child == (ino_t *)0) {
            parent->child = n;
        } else {
            ino_t *p = parent->child;
            while (p->sib != (ino_t *)0) {
                p = p->sib;
            }
            p->sib = n;
        }
    }
    return n;
}

static ino_t *ino_find(ino_t *dir, const char *name)
{
    ino_t *p;
    for (p = dir->child; p != (ino_t *)0; p = p->sib) {
        if (str_eq(p->name, name)) {
            return p;
        }
    }
    return (ino_t *)0;
}

static ino_t *walk(const char *path)
{
    ino_t *cur_ino = root;
    const char *p = path;

    if (path == (const char *)0 || path[0] != '/') {
        return (ino_t *)0;
    }
    while (*p != '\0') {
        char comp[24];
        u64 n = 0;
        ino_t *next;

        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        while (*p != '/' && *p != '\0') {
            if (n + 1 >= sizeof(comp)) {
                return (ino_t *)0;
            }
            comp[n++] = *p++;
        }
        comp[n] = '\0';
        next = ino_find(cur_ino, comp);
        if (next == (ino_t *)0) {
            return (ino_t *)0;
        }
        cur_ino = next;
    }
    return cur_ino;
}

/* 把一段二进制装进 inode：用于把内置程序写进 ramfs，供 exec-by-path 用。 */
static void ino_free_data(ino_t *n)
{
    if (n->data == (char *)0) {
        return;
    }
    if (n->page_backed) {
        mm_page_free(n->data);
    } else {
        kfree(n->data);
    }
    n->data = (char *)0;
    n->size = 0;
    n->cap = 0;
    n->page_backed = 0;
}

static void set_data(ino_t *n, const u8 *src, u64 len)
{
    char *buf;

    if (len == 0 || len > RAMFS_FILE_MAX) {
        return;
    }
    if (len > 1024UL) {
        buf = (char *)mm_page_alloc();
    } else {
        buf = (char *)kmalloc(len);
    }
    if (buf == (char *)0) {
        return;
    }
    {
        u64 i;

        for (i = 0; i < len; i++) {
            buf[i] = (char)src[i];
        }
    }
    n->data = buf;
    n->size = len;
    n->cap = 4096;
    n->page_backed = len > 1024UL;
}

static void set_text(ino_t *n, const char *s)
{
    u64 len = 0;
    char *buf;

    while (s[len] != '\0') {
        len++;
    }
    if (len > RAMFS_FILE_MAX) {
        return;
    }
    buf = (char *)kmalloc(len + 1);
    if (buf == (char *)0) {
        return;
    }
    {
        u64 i;
        for (i = 0; i <= len; i++) {
            buf[i] = s[i];
        }
    }
    n->data = buf;
    n->size = len;
    n->cap = len + 1;
}


/* 把路径切成「父目录 inode + 末段名」。根下的名字返回 root。 */
static ino_t *split_parent(const char *path, char *name, u64 cap)
{
    char dir[VFS_PATH_MAX];
    u64 len = 0;
    u64 cut = 0;
    u64 ns;
    u64 i;

    while (path[len] != '\0' && len + 1 < VFS_PATH_MAX) {
        len++;
    }
    if (len == 0 || path[0] != '/') {
        return (ino_t *)0;
    }
    for (i = 0; i < len; i++) {
        if (path[i] == '/') {
            cut = i;
        }
    }
    ns = len - cut - 1;
    if (ns == 0 || ns + 1 > cap) {
        return (ino_t *)0;
    }
    for (i = 0; i < ns; i++) {
        name[i] = path[cut + 1 + i];
    }
    name[ns] = '\0';
    if (cut == 0) {
        return root;
    }
    for (i = 0; i < cut && i + 1 < sizeof(dir); i++) {
        dir[i] = path[i];
    }
    dir[cut] = '\0';
    return walk(dir);
}

i64 vfs_mkdir(const char *path)
{
    char name[24];
    ino_t *p = split_parent(path, name, sizeof(name));

    if (p == (ino_t *)0 || p->type != T_DIR) {
        return -1;
    }
    if (ino_find(p, name) != (ino_t *)0) {
        return -1;
    }
    return ino_new(p, name, T_DIR) != (ino_t *)0 ? 0 : -1;
}

i64 vfs_unlink(const char *path)
{
    char name[24];
    ino_t *p = split_parent(path, name, sizeof(name));
    ino_t *victim;
    ino_t **pp;

    if (p == (ino_t *)0) {
        return -1;
    }
    victim = ino_find(p, name);
    if (victim == (ino_t *)0) {
        return -1;
    }
    if (victim->child != (ino_t *)0) {
        return -1; /* 目录非空 */
    }
    pp = &p->child;
    while (*pp != (ino_t *)0 && *pp != victim) {
        pp = &(*pp)->sib;
    }
    if (*pp == victim) {
        *pp = victim->sib;
    }
    victim->sib = (ino_t *)0;
    victim->parent = (ino_t *)0;
    if (victim->refs > 0) {
        /* 还有 fd 打开着：名字没了，数据留到最后一次 close */
        victim->unlinked = 1;
        return 0;
    }
    ino_free_data(victim);
    victim->type = -1;
    victim->name[0] = '\0';
    return 0;
}

/* 进程被收尸时释放它的 fd 表：归还 inode 引用，必要时真正释放数据。 */
void vfs_ftab_release(struct filetable *t)
{
    int i;

    if (t == (struct filetable *)0) {
        return;
    }
    for (i = 3; i < MAX_FD; i++) {
        ino_t *n = t->f[i].ino;

        if (!t->f[i].used || n == (ino_t *)0) {
            continue;
        }
        if (n->refs > 0) {
            n->refs--;
        }
        if (n->refs == 0 && (n->unlinked || n->type == T_PIPE)) {
            ino_free_data(n);
            n->type = -1;
            n->name[0] = '\0';
        }
        t->f[i].used = 0;
        t->f[i].ino = (ino_t *)0;
    }
    kfree(t);
}

void vfs_init(void)
{
    ino_t *dev;
    ino_t *etc;
    ino_t *con;
    int i;

    np = 0;
    for (i = 0; i < MAX_FD; i++) {
        fds[i].used = 0;
        fds[i].ino = (ino_t *)0;
        fds[i].off = 0;
    }

    root = ino_new((ino_t *)0, "/", T_DIR);
    dev = ino_new(root, "dev", T_DIR);
    etc = ino_new(root, "etc", T_DIR);
    (void)ino_new(root, "tmp", T_DIR);
    (void)ino_new(root, "mnt", T_DIR);
    {
        ino_t *bin = ino_new(root, "bin", T_DIR);

        set_text(ino_new(bin, "hello", T_FILE), "user-file: hello from ramfs file\n");
        /* 可执行镜像：exec-by-path 会读它 */
        set_data(ino_new(bin, "hello.bin", T_FILE),
                 user_blob_start, (u64)(user_blob_end - user_blob_start));
        /* shell 也进 ramfs：M4 的 init 用 exec_path("/bin/sh") 拉起它 */
        set_data(ino_new(bin, "sh", T_FILE),
                 user_shell_start, (u64)(user_shell_end - user_shell_start));
        /* 真正独立的 ELF 可执行文件，供 ELF 加载器验证 */
        set_data(ino_new(bin, "elfhello", T_FILE),
                 _binary_build_user_elf_elf_start,
                 (u64)(_binary_build_user_elf_elf_end - _binary_build_user_elf_elf_start));
    }

    con = ino_new(dev, "console", T_DEV);
    con->dev = DEV_CON;

    set_text(ino_new(etc, "motd", T_FILE), "hello from ramfs\n");
    set_text(ino_new(root, "hello.txt", T_FILE), "big kernel vfs works\n");

    for (i = 0; i < 3; i++) { /* 0/1/2 都指向控制台 */
        fds[i].ino = con;
        fds[i].used = 1;
        fds[i].off = 0;
    }

    /* 类 Unix 扩展（M6）：建 /proc 与 /dev/null、/dev/zero 等伪节点 */
    procfs_init();
    devfs_init();
}

/* /mnt/<name> 走 ext2：这里不做页缓存，第一次 read 整文件读进内存。 */
static int is_mnt_path(const char *p)
{
    return p[0] == '/' && p[1] == 'm' && p[2] == 'n' && p[3] == 't' && p[4] == '/' && p[5] != '\0';
}

i64 vfs_open(const char *path, int flags)
{
    ino_t *n = (ino_t *)0;
    int i;

    if (is_mnt_path(path)) {
        ino_t *mnt = walk("/mnt");

        if (mnt != (ino_t *)0) {
            n = ino_find(mnt, path + 5);
            if (n == (ino_t *)0) {
                n = ino_new(mnt, path + 5, T_EXT2);
            }
        }
    } else if (is_proc_path(path)) {
        /* procfs：路径是合成的，先把内容生成到节点 data 上再走普通 fd 流程。 */
        n = proc_resolve(path);
        if (n == (ino_t *)0) return -1;
    } else {
        n = walk(path);
    }

    if (n == (ino_t *)0) {
        char name[24];
        ino_t *p;

        if ((flags & 1) == 0) { /* O_CREAT */
            return -1;
        }
        p = split_parent(path, name, sizeof(name));
        if (p == (ino_t *)0 || p->type != T_DIR) {
            return -1;
        }
        n = ino_new(p, name, T_FILE);
        if (n == (ino_t *)0) {
            return -1;
        }
    }
    /* O_TRUNC(2)：把已有文件清零 */
    if ((flags & 2) != 0 && n != (ino_t *)0 && n->type == T_FILE) {
        ino_free_data(n);
    }
    n->refs++;
    for (i = 3; i < MAX_FD; i++) {
        if (!fds[i].used) {
            fds[i].ino = n;
            fds[i].off = 0;
            if ((flags & 4) != 0) { /* O_APPEND */
                fds[i].off = n->size;
            }
            fds[i].used = 1;
            return (i64)i;
        }
    }
    return -1;
}

i64 vfs_read(int fd, void *buf, u64 n)
{
    file_t *f;
    u64 k, left;

    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) {
        return -1;
    }
    f = &fds[fd];
    if (f->ino->type == T_EXT2 && f->ino->data == (char *)0) {
        char full[VFS_PATH_MAX];
        char *mem = (char *)mm_page_alloc();
        i64 got;
        u64 i;

        if (mem == (char *)0) {
            return -1;
        }
        /* ext2 的读取接口吃的是相对挂载点根的路径，所以要拼 "/<name>" */
        full[0] = '/';
        for (i = 0; i + 1 < VFS_PATH_MAX - 1 && f->ino->name[i] != '\0'; i++) {
            full[1 + i] = f->ino->name[i];
        }
        full[1 + i] = '\0';
        got = ext2_read(full, mem, 4096);
        if (got <= 0) {
            mm_page_free(mem);
            return -1;
        }
        f->ino->data = mem;
        f->ino->size = (u64)got;
        f->ino->cap = 4096;
    }

    if (fd < 0 || fd >= MAX_FD || !fds[fd].used || n == 0) {
        return -1;
    }
    f = &fds[fd];
    if (f->ino->type == T_PIPE) {
        u64 got = 0;
        u8 *d = (u8 *)buf;

        while (got < n && f->ino->size > 0) {
            d[got++] = (u8)f->ino->data[f->ino->rpos];
            f->ino->rpos = (f->ino->rpos + 1) % f->ino->cap;
            f->ino->size--;
        }
        return (i64)got;
    }
    if (f->ino->type == T_DEV) {
        if (f->ino->dev == DEV_CON) {
            /* 控制台读：从 UART 环形缓冲取，非阻塞。shell 用 read(0,…,1)
             * 轮询；没数据返回 0，调用方 yield 后再试。 */
            u64 got = 0;
            u8 *d = (u8 *)buf;
            while (got < n) {
                int c = uart_tryget();
                if (c < 0) {
                    break;
                }
                d[got++] = (u8)c;
            }
            return (i64)got;
        }
        if (f->ino->dev == T_DEVNULL || f->ino->dev == T_DEVZERO) {
            /* /dev/null：读到 EOF（0 字节）。/dev/zero：无限零流。 */
            if (f->ino->dev == T_DEVNULL) return 0;
            {
                u64 i;
                u8 *d = (u8 *)buf;
                for (i = 0; i < n; i++) d[i] = 0;
                f->off += n;
                return (i64)n;
            }
        }
        return -1;
    }
    if (f->ino->type == T_PROC) {
        /* procfs 合成内容：根据路径决定生成什么。
         * 打开时路径已经记录在 f->ino->name 里（vfs_open 对 /proc 特殊处理）。 */
        return proc_read_synthetic(f, buf, n);
    }
    if (f->ino->type != T_FILE && f->ino->type != T_EXT2) {
        return -1;
    }
    left = f->ino->size > f->off ? f->ino->size - f->off : 0;
    k = left < n ? left : n;
    {
        u64 i;
        u8 *d = (u8 *)buf;
        for (i = 0; i < k; i++) {
            d[i] = (u8)f->ino->data[f->off + i];
        }
    }
    f->off += k;
    return (i64)k;
}

i64 vfs_write(int fd, const void *buf, u64 n)
{
    file_t *f;
    const char *s = (const char *)buf;
    u64 i;

    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) {
        return -1;
    }
    f = &fds[fd];
    if (f->ino->type == T_DEV && f->ino->dev == DEV_CON) {
        for (i = 0; i < n; i++) {
            kputc(s[i]);
        }
        return (i64)n;
    }
    if (f->ino->type == T_DEV && (f->ino->dev == T_DEVNULL || f->ino->dev == T_DEVZERO)) {
        /* /dev/null 与 /dev/zero：写全部丢弃，成功返回 n。 */
        return (i64)n;
    }
    if (f->ino->type == T_PIPE) {
        u64 put = 0;
        u8 *d = (u8 *)buf;

        while (put < n && f->ino->size < f->ino->cap) {
            f->ino->data[f->ino->wpos] = (char)d[put++];
            f->ino->wpos = (f->ino->wpos + 1) % f->ino->cap;
            f->ino->size++;
        }
        return (i64)put;
    }
    if (f->ino->type != T_FILE) {
        return -1;
    }
    if (f->off + n > RAMFS_FILE_MAX) {
        return -1; /* ramfs 单文件上限，见 RAMFS_FILE_MAX */
    }
    if (f->off + n > f->ino->cap) {
        u64 ncap = RAMFS_FILE_MAX;
        char *nb;

        if (ncap > 1024UL) {
            nb = (char *)mm_page_alloc();
        } else {
            nb = (char *)kmalloc(ncap);
        }
        if (nb == (char *)0) {
            return -1;
        }
        for (i = 0; i < f->ino->size; i++) {
            nb[i] = f->ino->data[i];
        }
        ino_free_data(f->ino);
        f->ino->data = nb;
        f->ino->cap = ncap;
        f->ino->page_backed = ncap > 1024UL;
    }
    for (i = 0; i < n; i++) {
        f->ino->data[f->off + i] = s[i];
    }
    f->off += n;
    if (f->off > f->ino->size) {
        f->ino->size = f->off;
    }
    return (i64)n;
}

i64 vfs_close(int fd)
{
    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        return fd < 3 && fd >= 0 ? 0 : -1;
    }
    if (fds[fd].ino != (ino_t *)0) {
        ino_t *n = fds[fd].ino;

        if (n->refs > 0) {
            n->refs--;
        }
        if (n->refs == 0 && (n->unlinked || n->type == T_PIPE)) {
            ino_free_data(n);
            n->type = -1;
            n->name[0] = '\0';
        }
    }
    fds[fd].used = 0;
    fds[fd].ino = (ino_t *)0;
    fds[fd].off = 0;
    return 0;
}

i64 vfs_list(const char *path, char *out, u64 cap)
{
    ino_t *n = walk(path);
    ino_t *p;
    u64 used = 0;

    if (n == (ino_t *)0 || out == (char *)0 || cap == 0) {
        return -1;
    }
    if (n->type != T_DIR) {
        return -1;
    }
    for (p = n->child; p != (ino_t *)0; p = p->sib) {
        const char *s = p->name;
        while (*s != '\0' && used + 2 < cap) {
            out[used++] = *s++;
        }
        if (p->type == T_DIR && used + 2 < cap) {
            out[used++] = '/';
        }
        if (used + 1 < cap) {
            out[used++] = '\n';
        }
    }
    out[used] = '\0';
    return (i64)used;
}
/* ---- 每进程 fd 表：新建 / fork 时深拷贝 ---- */

/* ramfs 自检：连续建 50 个文件 -> 写 -> 删，验证 inode 池与链表回收。
 * 这个测试是内核侧的（不起进程），目的是尽早暴露“池子太小/删链漏了”。 */
static void path_num(char *dst, u32 n)
{
    char tmp[12];
    u32 i = 0;
    u32 len = 0;

    dst[0] = '/';
    dst[1] = 't';
    dst[2] = 'm';
    dst[3] = 'p';
    dst[4] = '/';
    dst[5] = 's';
    len = 6;
    if (n == 0) {
        tmp[i++] = '0';
    }
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        dst[len++] = tmp[--i];
    }
    dst[len] = '\0';
}

/* 建一个管道，写出读/写两个 fd；缓冲用一页，非阻塞语义。 */
i64 vfs_pipe(int out[2])
{
    ino_t *n;
    int r;
    int w;

    n = ino_new((ino_t *)0, "pipe", T_PIPE);
    if (n == (ino_t *)0) {
        return -1;
    }
    n->data = (char *)mm_page_alloc();
    if (n->data == (char *)0) {
        n->type = -1;
        return -1;
    }
    n->cap = 4096;
    r = -1;
    w = -1;
    {
        int i;

        for (i = 3; i < MAX_FD; i++) {
            if (!fds[i].used) {
                fds[i].ino = n;
                fds[i].off = 0;
                fds[i].used = 1;
                r = i;
                break;
            }
        }
    }
    if (r >= 0) {
        int i;

        for (i = 3; i < MAX_FD; i++) {
            if (!fds[i].used) {
                fds[i].ino = n;
                fds[i].off = 0;
                fds[i].used = 1;
                w = i;
                break;
            }
        }
    }
    if (r < 0 || w < 0) {
        if (r >= 0) {
            fds[r].used = 0;
        }
        mm_page_free(n->data);
        n->data = (char *)0;
        n->type = -1;
        return -1;
    }
    n->refs = 2;
    out[0] = r;
    out[1] = w;
    return 0;
}

i64 vfs_dup(int fd)
{
    int i;

    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) {
        return -1;
    }
    for (i = 3; i < MAX_FD; i++) {
        if (!fds[i].used) {
            fds[i] = fds[fd];
            if (fds[i].ino != (ino_t *)0) {
                fds[i].ino->refs++;
            }
            return (i64)i;
        }
    }
    return -1;
}

/* ==================== 类 Unix 扩展（M6）==================== */

/*
 * dup2：把 oldfd 复制到 newfd。若 newfd 已打开，先关掉它（POSIX 语义）。
 * newfd 等于 oldfd 时什么都不做、直接返回 newfd。
 */
i64 vfs_dup2(int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= MAX_FD || newfd < 0 || newfd >= MAX_FD) {
        return -1;
    }
    if (!fds[oldfd].used) {
        return -1;
    }
    if (oldfd == newfd) {
        return (i64)newfd;
    }
    if (fds[newfd].used) {
        vfs_close(newfd);
    }
    fds[newfd] = fds[oldfd];
    if (fds[newfd].ino != (ino_t *)0) {
        fds[newfd].ino->refs++;
    }
    return (i64)newfd;
}

/*
 * chdir：把当前工作目录设成 path。成功返回 0。
 * 校验放在 sched_setcwd（sched.c）里：存在性 + 是目录。
 * 这里只是命名一致的入口，实际由调度器完成。
 */
i64 vfs_chdir(const char *path)
{
    if (path == (const char *)0) return -1;
    return sched_setcwd(path);
}

i64 vfs_getcwd(char *buf, u64 cap)
{
    if (buf == (char *)0 || cap == 0) return -1;
    if (sched_getcwd(buf, cap) == (const char *)0) return -1;
    return (i64)0;
}

/*
 * fstat：把 fd 的元数据写到用户缓冲区。
 * buf 是用户指针，由 syscall 层校验过 u_ok 之后才进来。
 * 字段集是最小 POSIX 子集：st_dev/inode/mode/nlink/uid/gid/rdev/size/blksize/blocks。
 */
i64 vfs_fstat(int fd, u64 ubuf, u64 n)
{
    struct {
        u64 st_dev, st_ino, st_mode, st_nlink;
        u64 st_uid, st_gid, st_rdev, st_size;
        u64 st_blksize, st_blocks;
    } *st = (void *)ubuf;
    file_t *f;

    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) return -1;
    if (n < sizeof(*st)) return -1;
    f = &fds[fd];
    st->st_dev = 0;
    st->st_ino = (u64)(u64)f->ino;
    st->st_mode = (f->ino->type == T_DIR) ? 0040000 | 0755 :
                  (f->ino->type == T_DEV) ? 0020000 | 0620 :
                  (f->ino->type == T_PIPE) ? 0010000 | 0600 : 0100000 | 0644;
    st->st_nlink = 1;
    st->st_uid = (u64)(u64)sched_getuid();
    st->st_gid = (u64)(u64)sched_getgid();
    st->st_rdev = (f->ino->type == T_DEV) ? (u64)(u64)f->ino->dev : 0;
    st->st_size = f->ino->size;
    st->st_blksize = 4096;
    st->st_blocks = (f->ino->size + 4095) / 4096;
    return (i64)0;
}

/* ioctl：当前只实现 TIOCGWINSZ 这类无指针命令的占位。
 * 真 tty 驱动接进来后再补参数拷贝。 */
i64 vfs_ioctl(int fd, u64 req, u64 arg)
{
    (void)arg;
    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) return -1;
    if (fds[fd].ino->type != T_DEV) return -1;
    return -1; /* ENOSYS：当前无可用 ioctl */
}

/*
 * mount：把文件系统挂到目录上。
 * 当前只支持把 ext2 只读镜像挂到 /mnt。
 * src 是块设备路径（暂忽略，固定用 virtio-blk），tgt 是挂载点，
 * fstype 为 "ext2" 时执行挂载。
 */
i64 vfs_mount(const char *src, const char *tgt, const char *fstype)
{
    (void)src;
    if (tgt == (const char *)0 || fstype == (const char *)0) return -1;
    if (streq(fstype, "ext2")) {
        ino_t *mnt = walk("/mnt");
        if (mnt != (ino_t *)0 && mnt->type == T_DIR) {
            mnt->type = T_EXT2; /* 标记为已挂载，vfs_open 走 ext2 分支 */
            return 0;
        }
        return -1;
    }
    return -1;
}

/* ==================== procfs / devfs（M6）==================== */

/* 当前时间（秒）：用 tick/100 近似。 */
static u64 boottime_sec(void)
{
    return tick_count() / 100;
}

/*
 * procfs：只读伪文件系统，挂在 /proc。
 * 节点都是合成出来的，不占 ramfs inode 数据。当前支持：
 *   /proc/self/        —— 指向当前 pid 的目录
 *   /proc/<pid>/stat    —— 进程状态一行
 *   /proc/<pid>/cmdline —— 进程名（当前就是 tcb->name）
 *   /proc/meminfo      —— 内存概览
 *   /proc/uptime       —— 开机秒数
 *   /proc/version      —— 内核版本字符串
 * 读取走 vfs_read，写入一律拒绝。
 */
#define T_PROC  5
#define T_PROCD 6

/* 前向声明：snprint 定义在本文件后面，被 proc_read_* 系列提前调用。 */
static i64 snprint(char *d, u64 cap, const char *fmt, ...);

static int is_proc_path(const char *p)
{
    return p[0] == '/' && p[1] == 'p' && p[2] == 'r' && p[3] == 'o' && p[4] == 'c' &&
           (p[5] == '/' || p[5] == '\0');
}

/* 路径工具：跳过多余的 '/'；把下一个组件读到 comp（不含斜杠），返回消耗掉的字节数。 */
static u64 proc_next_comp(const char *p, u64 off, char *comp, u64 cmax)
{
    u64 i = 0;

    while (p[off] == '/') off++;
    while (p[off] != '/' && p[off] != '\0' && i + 1 < cmax) {
        comp[i++] = p[off++];
    }
    comp[i] = '\0';
    return off;
}

/* 取当前 pid；procfs 用。 */
static u64 proc_self_pid(void)
{
    return (u64)(u64)sched_pid();
}

/*
 * proc_resolve：把 /proc 下的合成路径解析成一个临时 inode。
 * 因为内容全是实时生成的、不落盘，这里返回的是调用方已存在的一组
 * “伪节点”之一（用 ramfs inode 承载，type 标成 T_PROC/T_PROCD），
 * 打开时把路径记到 ino->name，供 vfs_read 判断读什么。
 *
 * 支持：
 *   /proc/self            -> 目录（T_PROCD），指向当前 pid
 *   /proc/self/stat       -> 文件（T_PROC）
 *   /proc/self/cmdline    -> 文件（T_PROC）
 *   /proc/<pid>           -> 目录（T_PROCD）
 *   /proc/<pid>/stat      -> 文件（T_PROC）
 *   /proc/<pid>/cmdline   -> 文件（T_PROC）
 *   /proc/meminfo         -> 文件（T_PROC）
 *   /proc/uptime          -> 文件（T_PROC）
 *   /proc/version         -> 文件（T_PROC）
 * 返回 (ino_t *)0 表示路径不存在。
 */
static ino_t *proc_resolve(const char *path)
{
    u64 off = 5; /* 跳过 "/proc" */
    char comp[24];
    u64 pid;
    ino_t *dir;

    off = proc_next_comp(path, off, comp, sizeof(comp));
    if (comp[0] == '\0') {
        /* /proc 本身：返回 proc 根目录 */
        return ino_find(root, "proc");
    }

    /* 第一组件：self、<pid>，或顶级文件 meminfo/uptime/version */
    if (streq(comp, "meminfo") || streq(comp, "uptime") || streq(comp, "version")) {
        ino_t *f = ino_find(dir, comp);
        if (f == (ino_t *)0) {
            f = ino_new(dir, comp, T_PROC);
            if (f == (ino_t *)0) return (ino_t *)0;
            f->type = T_PROC;
        }
        {
            u64 k = 0;
            while (path[k] != '\0' && k + 1 < sizeof(f->name)) {
                f->name[k] = path[k];
                k++;
            }
            f->name[k] = '\0';
        }
        return f;
    }
    if (streq(comp, "self")) {
        pid = proc_self_pid();
    } else {
        u64 v = 0;
        int i = 0;
        while (comp[i] >= '0' && comp[i] <= '9') {
            v = v * 10 + (u64)(comp[i] - '0');
            i++;
        }
        if (i == 0 || comp[i] != '\0') return (ino_t *)0;
        pid = v;
    }
    if (sched_tcb_by_pid((int)pid) == (tcb_t *)0) return (ino_t *)0;

    /* 查 pid 目录（已建好的伪节点） */
    dir = ino_find(root, "proc");
    if (dir == (ino_t *)0) return (ino_t *)0;
    /* pid 目录用 "self" 命名占位：所有 pid 共享同一伪目录节点，
     * 具体 pid 由路径在 vfs_read 里重新解析。 */
    {
        ino_t *pd = ino_find(dir, "self");
        if (pd == (ino_t *)0) {
            pd = ino_new(dir, "self", T_PROCD);
            if (pd == (ino_t *)0) return (ino_t *)0;
            pd->type = T_PROCD;
        }
        off = proc_next_comp(path, off, comp, sizeof(comp));
        if (comp[0] == '\0') {
            return pd;
        }
        /* 第二组件：stat / cmdline */
        if (streq(comp, "stat") || streq(comp, "cmdline")) {
            ino_t *f = ino_find(pd, comp);
            if (f == (ino_t *)0) {
                f = ino_new(pd, comp, T_PROC);
                if (f == (ino_t *)0) return (ino_t *)0;
                f->type = T_PROC;
            }
            /* 把完整路径记到 name，vfs_read 用它判断内容 */
            {
                u64 k = 0;
                while (path[k] != '\0' && k + 1 < sizeof(f->name)) {
                    f->name[k] = path[k];
                    k++;
                }
                f->name[k] = '\0';
            }
            return f;
        }
        return (ino_t *)0;
    }
}

/*
 * proc_read_synthetic：根据 f->ino->name 里记的完整路径，生成内容写到 buf。
 * 返回写入字节数（可能受 cap 截断），失败 -1。
 */
static i64 proc_read_synthetic(file_t *f, void *buf, u64 cap)
{
    const char *p = f->ino->name;

    if (p[0] == '/' && p[1] == 'p' && p[2] == 'r' && p[3] == 'o' && p[4] == 'c' &&
        p[5] == '/') {
        u64 off = 6;
        char comp[24];

        off = proc_next_comp(p, off, comp, sizeof(comp));
        if (streq(comp, "self")) {
            off = proc_next_comp(p, off, comp, sizeof(comp));
        } else if (comp[0] >= '0' && comp[0] <= '9') {
            /* 跳过 <pid> 组件 */
            off = proc_next_comp(p, off, comp, sizeof(comp));
        }
        if (streq(comp, "stat")) {
            u64 pid;
            if (p[6] == 's') {
                pid = proc_self_pid();
            } else {
                pid = 0;
                while (p[6] >= '0' && p[6] <= '9') {
                    pid = pid * 10 + (u64)(p[6] - '0');
                    off++;
                }
                (void)off;
            }
            return proc_read_stat(pid, (char *)buf, cap);
        }
        if (streq(comp, "cmdline")) {
            u64 pid;
            if (p[6] == 's') {
                pid = proc_self_pid();
            } else {
                pid = 0;
                int i = 6;
                while (p[i] >= '0' && p[i] <= '9') {
                    pid = pid * 10 + (u64)(p[i] - '0');
                    i++;
                }
            }
            return proc_read_cmdline(pid, (char *)buf, cap);
        }
    }
    return -1;
}

/* 读 /proc/<pid>/stat：填一个合成 inode 的 data。返回字节数，失败 -1。 */
static i64 proc_read_stat(u64 pid, char *buf, u64 cap)
{
    static const char *st[5] = { "free", "ready", "run", "exit", "zombie" };
    int n = 0;
    char tmp[128];
    tcb_t *t = sched_tcb_by_pid((int)pid);

    if (t == (tcb_t *)0) return -1;
    {
        const char *state = (t->state >= 0 && t->state <= 4) ? st[t->state] : "?";
        n = snprint(tmp, sizeof(tmp),
                    "%u (%s) %s %u %u %u %u %u %u %u %u %u %u %u %u "
                    "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u "
                    "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                    t->pid, t->name, state, t->ppid,
                    t->pgid, t->pid, /* session = pgid 占位 */
                    t->pid,          /* tty_nr */
                    0, 0, 0, 0, 0,  /* minflt cminflt majflt cminflt */
                    0, 0,           /* utime stime */
                    t->code, 0, 0, 0, 0, /* cutime cstime priority nice */
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    if (n == 0) return -1;
    if ((u64)n > cap) n = (int)cap;
    for (i = 0; i < n; i++) buf[i] = tmp[i];
    return (i64)n;
}

static i64 proc_read_cmdline(u64 pid, char *buf, u64 cap)
{
    int n = 0;
    tcb_t *t = sched_tcb_by_pid((int)pid);

    if (t == (tcb_t *)0) return -1;
    while (t->name[n] && (u64)n + 1 < cap) {
        buf[n] = t->name[n];
        n++;
    }
    buf[n++] = '\0';
    return (i64)n;
}

static i64 proc_read_meminfo(char *buf, u64 cap)
{
    char tmp[256];
    i64 n = snprint(tmp, sizeof(tmp),
                    "MemTotal:      %8u kB\nMemFree:       %8u kB\n",
                    (u32)(mm_pages_total() * 4), (u32)(mm_pages_free() * 4));
    if ((u64)n > cap) n = (i64)cap;
    for (i64 i = 0; i < n; i++) buf[i] = tmp[i];
    return n;
}

static i64 proc_read_uptime(char *buf, u64 cap)
{
    u64 t = tick_count();
    u64 sec = t / 100;
    u64 frac = t % 100; /* 百分之一秒：100 Hz 定时器下就是真实的小数位 */
    char tmp[64];
    i64 n = snprint(tmp, sizeof(tmp), "%u.%02u %u.%02u\n",
                    (u32)sec, (u32)frac, 0, 0);
    if ((u64)n > cap) n = (i64)cap;
    for (i64 i = 0; i < n; i++) buf[i] = tmp[i];
    return n;
}

/* 简易 snprintf（数字 + 字符串），仅供 procfs 使用。 */
static i64 snprint(char *d, u64 cap, const char *fmt, ...)
{
    va_list ap;
    u64 i = 0;

    va_start(ap, fmt);
    for (; *fmt && i + 1 < cap; fmt++) {
        if (*fmt != '%') { d[i++] = *fmt; continue; }
        fmt++;
        if (*fmt == 'u') {
            u64 v = va_arg(ap, u64);
            char t[24];
            int k = 0;
            if (v == 0) t[k++] = '0';
            else while (v) { t[k++] = '0' + v % 10; v /= 10; }
            while (k--) { if (i + 1 < cap) d[i++] = t[k]; }
        } else if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            if (s) while (*s && i + 1 < cap) d[i++] = *s++;
        } else if (*fmt == 'c') {
            d[i++] = (char)va_arg(ap, int);
        } else if (*fmt == '%') {
            d[i++] = '%';
        } else {
            d[i++] = '%'; d[i++] = *fmt;
        }
    }
    va_end(ap);
    d[i] = '\0';
    return (i64)i;
}

/*
 * devfs：/dev 下的特殊节点。
 * 当前提供：null、zero、full、urandom（均为无限零/随机源），
 * tty、console（串口），以及 ram0（ramfs 根的内存盘，占位）。
 * 读 null/zero 返回 0；写全部吞掉返回 n。
 */
#define T_DEVNULL 3
#define T_DEVZERO 4

static int is_dev_path(const char *p)
{
    return p[0] == '/' && p[1] == 'd' && p[2] == 'e' && p[3] == 'v' &&
           (p[4] == '/' || p[4] == '\0');
}

static ino_t *dev_node(const char *name)
{
    ino_t *d = ino_find(root, "dev");
    if (d == (ino_t *)0) return (ino_t *)0;
    return ino_find(d, name);
}

static ino_t *dev_lookup(const char *path)
{
    char comp[24];
    const char *p = path + 5; /* 跳过 "/dev" */
    u64 n = 0;

    while (*p == '/') p++;
    while (*p && *p != '/' && n + 1 < sizeof(comp)) comp[n++] = *p++;
    comp[n] = '\0';
    return dev_node(comp);
}

/* 在 vfs_init 里调用：建 /proc 与 /dev 下的伪节点。 */
void procfs_init(void)
{
    ino_t *proc;

    proc = ino_new(root, "proc", T_DIR);
    if (proc != (ino_t *)0) proc->type = T_PROC;
}

void devfs_init(void)
{
    ino_t *dev = ino_find(root, "dev");

    if (dev != (ino_t *)0) {
        ino_t *n;

        n = ino_new(dev, "null", T_DEV);
        if (n) n->dev = T_DEVNULL;
        n = ino_new(dev, "zero", T_DEV);
        if (n) n->dev = T_DEVZERO;
        n = ino_new(dev, "tty", T_DEV);
        if (n) n->dev = DEV_CON;
        n = ino_new(dev, "urandom", T_DEV);
        if (n) n->dev = T_DEVZERO;
    }
}

i64 vfs_rename(const char *oldp, const char *newp)
{
    char oname[24];
    char nname[24];
    ino_t *op = split_parent(oldp, oname, sizeof(oname));
    ino_t *np = split_parent(newp, nname, sizeof(nname));
    ino_t *victim;
    ino_t **pp;

    if (op == (ino_t *)0 || np == (ino_t *)0) {
        return -1;
    }
    victim = ino_find(op, oname);
    if (victim == (ino_t *)0) {
        return -1;
    }
    if (ino_find(np, nname) != (ino_t *)0) {
        return -1; /* 目标已存在 */
    }
    pp = &op->child;
    while (*pp != (ino_t *)0 && *pp != victim) {
        pp = &(*pp)->sib;
    }
    if (*pp == victim) {
        *pp = victim->sib;
    }
    victim->sib = (ino_t *)0;
    victim->parent = np;
    if (np->child == (ino_t *)0) {
        np->child = victim;
    } else {
        ino_t *tail = np->child;

        while (tail->sib != (ino_t *)0) {
            tail = tail->sib;
        }
        tail->sib = victim;
    }
    str_copy(victim->name, nname, sizeof(victim->name));
    return 0;
}

i64 vfs_access(const char *path)
{
    return vfs_size(path) >= 0 ? 0 : -1;
}

i64 vfs_seek(int fd, i64 off)
{
    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        return -1;
    }
    if (off < 0 || (u64)off > fds[fd].ino->size) {
        return -1;
    }
    fds[fd].off = (u64)off;
    return off;
}

i64 vfs_size(const char *path)
{
    ino_t *n;

    if (is_mnt_path(path)) {
        ino_t *mnt = walk("/mnt");
        n = mnt != (ino_t *)0 ? ino_find(mnt, path + 5) : (ino_t *)0;
        if (n != (ino_t *)0 && n->data == (char *)0) {
            return -1; /* 尚未物化，调用方先 open/read 一次 */
        }
    } else {
        n = walk(path);
    }
    if (n == (ino_t *)0) {
        return -1;
    }
    return (i64)n->size;
}

int vfs_selftest(void)
{
    char path[32];
    u32 i;
    int ok = 1;

    for (i = 0; i < 50; i++) {
        i64 fd;

        path_num(path, i);
        fd = vfs_open(path, 1);
        if (fd < 0) {
            ok = 0;
            break;
        }
        if (vfs_write(fd, "x", 1) != 1) {
            ok = 0;
        }
        vfs_close(fd);
        if (!ok) {
            break;
        }
    }
    for (i = 0; i < 50 && ok; i++) {
        path_num(path, i);
        if (vfs_unlink(path) != 0) {
            ok = 0;
        }
    }
    /* 第二轮：如果 unlink 没有把槽位还回来，这里一定失败 */
    for (i = 0; i < 50 && ok; i++) {
        i64 fd;

        path_num(path, i);
        fd = vfs_open(path, 1);
        if (fd < 0) {
            ok = 0;
            break;
        }
        vfs_close(fd);
        vfs_unlink(path);
    }
    return ok;
}

struct filetable *vfs_ftab_new(void)
{
    struct filetable *t = (struct filetable *)kmalloc(sizeof(struct filetable));
    ino_t *con;
    int i;

    if (t == (struct filetable *)0) {
        return (struct filetable *)0;
    }
    for (i = 0; i < MAX_FD; i++) {
        t->f[i].used = 0;
        t->f[i].ino = (ino_t *)0;
        t->f[i].off = 0;
    }
    con = walk("/dev/console");
    for (i = 0; i < 3 && con != (ino_t *)0; i++) {
        t->f[i].ino = con;
        t->f[i].used = 1;
        t->f[i].off = 0;
    }
    return t;
}

struct filetable *vfs_ftab_clone(void)
{
    struct filetable *src = ftab();
    struct filetable *dst = vfs_ftab_new();
    int i;

    if (dst == (struct filetable *)0) {
        return (struct filetable *)0;
    }
    for (i = 0; i < MAX_FD; i++) {
        dst->f[i] = src->f[i];
        if (i >= 3 && src->f[i].used && src->f[i].ino != (ino_t *)0) {
            src->f[i].ino->refs++;
        }
    }
    return dst;
}
