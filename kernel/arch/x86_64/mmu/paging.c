/*
 * * paging.c ---- 64-bit ???? + ???????
 * *
 * * ??? AArch64 ???????
 * *   pgd_new        <-> pg_aspace_new
 * *   pgd_map        <-> pg_map_user
 * *   pgd_switch     <-> pg_activate
 * *   pgd_current    <-> sched_get_l1
 * *
 * * ??????
 * *   0x000000000000..0x00007FFFFFFFFFFF   ???????????
 * *   0xFFFF800000000000..0xFFFFFFFFFFFFFFFF ???????????
 * * ?????? 2 MiB0x400000..0x600000???? 2 MiB ????
 * * ????? ELF ??? USER_TOP???????
 * *
 * * ??????? mm_page_alloc? AArch64 ???? 4 KiB ????
 * * COW ???????mm_page_ref/unref??????????
 */
#include "x86.h"
#include "../../include/kern.h"

#define PGDIR_SH 39
#define PUDIR_SH 30
#define PMDIR_SH 21
#define PTAB_SH  12

#define PTE_SHIFT(n) (((n) >> 3) & 0x1FF)

static uint64_t *current_pgd = (uint64_t *)0;
static uint64_t  kernel_cr3 = 0;

/*
 * * pte_ptr ---- ??????create=1 ???????????
 * * ???????? pg_user_pa ????0 ?????
 */
static uint64_t *pte_ptr(uint64_t *pgd, uint64_t v, int create) {
    uint64_t *pud, *pmd, *pt;
    uint64_t  e;

    e = pgd[PTE_SHIFT(v >> PGDIR_SH)];
    if (!(e & PTE_P)) {
        if (!create) return (uint64_t *)0;
        pud = (uint64_t *)mm_page_alloc();
        if (!pud) return (uint64_t *)0;
        pgd[PTE_SHIFT(v >> PGDIR_SH)] = ((uint64_t)pud & ~0xFFFULL) | PTE_P | PTE_RW;
    } else {
        pud = (uint64_t *)((e & ~0xFFFULL) & ~PTE_NX);
    }

    e = pud[PTE_SHIFT(v >> PUDIR_SH)];
    if (!(e & PTE_P)) {
        if (!create) return (uint64_t *)0;
        pmd = (uint64_t *)mm_page_alloc();
        if (!pmd) return (uint64_t *)0;
        pud[PTE_SHIFT(v >> PUDIR_SH)] = ((uint64_t)pmd & ~0xFFFULL) | PTE_P | PTE_RW;
    } else {
        pmd = (uint64_t *)((e & ~0xFFFULL) & ~PTE_NX);
    }

    e = pmd[PTE_SHIFT(v >> PMDIR_SH)];
    if (!(e & PTE_P)) {
        if (!create) return (uint64_t *)0;
        pt = (uint64_t *)mm_page_alloc();
        if (!pt) return (uint64_t *)0;
        pmd[PTE_SHIFT(v >> PMDIR_SH)] = ((uint64_t)pt & ~0xFFFULL) | PTE_P | PTE_RW;
    } else {
        pt = (uint64_t *)((e & ~0xFFFULL) & ~PTE_NX);
    }

    return &pt[PTE_SHIFT(v >> PTAB_SH)];
}

uint64_t *pgd_new(void) {
    uint64_t *pgd = (uint64_t *)mm_page_alloc();
    if (!pgd) return (uint64_t *)0;

 * ????????PML4[256..511]M0 ??????????
 * * 0..4 GiB????? PML4[256]paging_init ?????
    pgd[256] = current_pgd ? current_pgd[256] : 0;
    return pgd;
}

void pgd_free(uint64_t *pgd) {
    if (pgd && pgd != current_pgd)
        mm_page_free(pgd);
}

void pgd_map(uint64_t *pgd, uint64_t v, uint64_t p, uint64_t flags) {
    uint64_t *pte = pte_ptr(pgd, v, 1);
    if (!pte) return;
    *pte = (p & ~0xFFFULL) | (flags & ~PTE_NX);
    if (!(flags & PTE_RW))
        *pte |= PTE_NX;                     /* ???????? */
    if (pgd == current_pgd)
        invlpg(v);
}

uint64_t pgd_walk(uint64_t *pgd, uint64_t v, int create) {
    uint64_t *pte = pte_ptr(pgd, v, create);
    if (!pte || !(*pte & PTE_P)) return 0;
    return (*pte & ~0xFFFULL) | ((uint64_t)pgd & 0xFFFULL);
}

void pgd_switch(uint64_t *pgd) {
    if (pgd == current_pgd) return;
    current_pgd = pgd;
    write_cr3((uint64_t)pgd);
}

uint64_t *pgd_current(void) {
    return current_pgd;
}

/*
 * * paging_init ---- ??? 2 MiB ????????????
 * * ??
 * *   phys_free ??????????????????
 * *   phys_end  ?????????? Multiboot2 mmap????? 128 MiB
 * * ????
 * *   [0..phys_end)        -> KERNEL_VMA + [0..phys_end)   ???RW
 * *   [0..2 MiB)           -> [0..2 MiB)                   ????????
 * *   [USER_BASE..USER_TOP)-> ???? mm ??           ???
 * * ??? WP ?CR0.WP????????????----?? COW ???
 */
void paging_init(uint64_t phys_free, uint64_t phys_end) {
    uint64_t *pgd = pgd_new();
    if (!pgd) panic("paging: pgd alloc failed");

 * PML4[0] = ? 512 GiB ???????????????? PDPT
    pgd[0] = ((uint64_t)0x71000) | PTE_P | PTE_RW;

 * ??????? PML4[256]?? PDPT ??? PD
    uint64_t *pdpt_k = (uint64_t *)mm_page_alloc();
    pgd[256] = (uint64_t)pdpt_k | PTE_P | PTE_RW;

    uint64_t *pd_k = (uint64_t *)mm_page_alloc();
    pdpt_k[0] = (uint64_t)pd_k | PTE_P | PTE_RW;
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t p = i * 0x200000ULL;
        if (p >= phys_end) break;
        pd_k[i] = p | PTE_P | PTE_RW | PTE_PS;
    }

 * ??????? KERNEL_VMA ?? 4 MiB ?????
    uint64_t *pd_id = (uint64_t *)mm_page_alloc();
    pdpt_k[1] = (uint64_t)pd_id | PTE_P | PTE_RW;
    for (uint64_t i = 0; i < 2; i++)
        pd_id[i] = (i * 0x200000ULL) | PTE_P | PTE_RW | PTE_PS;

    pgd_switch(pgd);
    kernel_cr3 = (uint64_t)pgd;

    uint64_t cr0 = read_cr0();
    write_cr0(cr0 | (1ULL << 16));          /* WP=1 */

    mm_init();
}

/*
 * * x86 ??? kern.h ???????mm ? sched ??? core/ ???
 * * ????????????????????????M0 ???
 */
void *mm_page_alloc(void)   { return 0; }
void  mm_page_free(void *p) { (void)p; }
void  mm_init(void)         { }

void pg_init(void) { }
int pg_selftest(void) { return 0; }

void *pg_template(void)           { return current_pgd; }
uint64_t *pg_aspace_new(void)     { return pgd_new(); }
uint64_t *pg_aspace_clone(uint64_t *src) {
    if (!src) return pgd_new();
    return src;                                   /* TODO(x86): ??? */
}
int  pg_map_user(uint64_t *l1, uint64_t va, uint64_t pa, uint64_t attr) {
    pgd_map(l1, va, pa, attr | PTE_US);
    return 0;
}
uint64_t pg_user_pa(uint64_t *l1, uint64_t va) {
    (void)l1;
    uint64_t *pte = pte_ptr(current_pgd, va, 0);
    if (!pte || !(*pte & PTE_P)) return 0;
    return *pte & ~0xFFFULL;
}
void pg_activate(uint64_t *l1)    { pgd_switch(l1); }
void pg_aspace_ref(uint64_t *l1)  { (void)l1; }
void pg_aspace_unref(uint64_t *l1){ pgd_free(l1); }
void mm_page_set_cow(void *p, int on) { (void)p; (void)on; }
int  mm_page_cow(void *p)              { (void)p; return 0; }
int  pg_cow_fault(uint64_t far)        { (void)far; return 0; }
uint64_t pg_cow_faults(void)           { return 0; }
void pg_cow_stats(uint64_t *c, uint64_t *s) { if (c) *c = 0; if (s) *s = 0; }
