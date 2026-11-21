/*
 * string.c —— 字符串与内存例程。
 * 全部自包含，不依赖任何外部实现；memset/memcpy 直接用字节循环，
 * 因为 EL0 里我们没有 SIMD、也不保证对齐。
 */

#include "libc.h"

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != 0) ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst + strlen(dst);
    while ((*d++ = *src++) != 0) ;
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (char)c == 0 ? (char *)s : (char *)0;
}

char *strrchr(const char *s, int c)
{
    const char *last = (char *)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == 0) last = s;
    return (char *)last;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

int memcmp(const void *a, void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *d = (char *)sys_mmap((void *)0, len, 3, 0x20, -1, 0); /* PROT_RW | MAP_PRIVATE_ANON */
    if (d == (char *)-1) return (char *)0;
    memcpy(d, s, len);
    return d;
}

/*
 * strtok_r：状态存在 save 里，重复调用传 NULL。
 * 返回的 token 会就地改成 '\0'，所以原串会被破坏。
 */
char *strtok_r(char *s, const char *delim, char **save)
{
    char *spanp;
    int c, sc;
    char *tok;

    if (s == (char *)0 && (s = *save) == (char *)0) return (char *)0;

    /*
     * 跳过前导分隔符。这里用 strchr 模拟：逐字符查 delim。
     */
    c = *s++;
    for (; c; c = *s++) {
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) break;
        } while (sc);
        if (!sc) break;
    }
    if (!c) { *save = (char *)0; return (char *)0; }
    tok = s - 1;

    for (; c; c = *s++) {
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                s[-1] = 0;
                *save = s;
                return tok;
            }
        } while (sc);
    }
    *save = (char *)0;
    return tok;
}
