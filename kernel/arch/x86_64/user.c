#include <stddef.h>
#include <stdint.h>
#include "user.h"

#define USER_VA_BASE       0x00400000ULL
#define USER_VA_LIMIT      0x00600000ULL
#define USER_PHYS_BASE     0x02000000ULL
#define USER_STACK_VA      0x7fe00000ULL
#define USER_STACK_PHYS    0x03000000ULL
#define USER_STACK_SIZE    0x4000ULL
#define USER_STACK_TOP     (USER_STACK_VA + USER_STACK_SIZE)
#define PAGE_SIZE          4096ULL
#define PTE_P              1ULL
#define PTE_W              2ULL
#define PTE_U              4ULL
#define CPIO_MODULE_TAG    3U
#define MB2_TAG_END        0U
#define MB2_TAG_MODULE     3U
#define ELF_PT_LOAD        1U
#define ELF_PF_X           1U
#define ELF_PF_W           2U
#define ALPOS_SYS_EXIT     0
#define ALPOS_SYS_WRITE    1
#define EXIT_SENTINEL      (-4096L)
#define USER_EXEC_ERROR    (-256)

struct mb2_tag { uint32_t type, size; } __attribute__((packed));
struct mb2_module { uint32_t type, size, mod_start, mod_end; char cmdline[]; } __attribute__((packed));
struct cpio_file { const uint8_t *data; uint32_t size; };
struct elf64_ehdr {
    unsigned char ident[16]; uint16_t type, machine; uint32_t version;
    uint64_t entry, phoff, shoff; uint32_t flags; uint16_t ehsize, phentsize;
    uint16_t phnum, shentsize, shnum, shstrndx;
} __attribute__((packed));
struct elf64_phdr {
    uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align;
} __attribute__((packed));
struct syscall_frame {
    uint64_t rax, rbx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
};
extern void enter_user(uint64_t entry, uint64_t user_rsp);
volatile int user_exit_status;

static uint8_t *const code_pt = (uint8_t *)0x76000;
static uint8_t *const stack_pt = (uint8_t *)0x77000;
static uint64_t module_start, module_end;

static void *mem_set(void *dst, int value, size_t n) {
    uint8_t *d = (uint8_t *)dst; size_t i;
    for (i=0;i<n;i++) d[i]=(uint8_t)value;
    return dst;
}
static void mem_copy(void *dst, const void *src, size_t n) {
    uint8_t *d=(uint8_t *)dst; const uint8_t *s=(const uint8_t *)src; size_t i;
    for(i=0;i<n;i++) d[i]=s[i];
}
static int str_eq(const char *a,const char *b) {
    while(*a && *b && *a==*b){++a;++b;} return *a==*b;
}
static size_t str_len(const char *s) { size_t n=0; while(s[n])++n; return n; }
static unsigned hex8(const uint8_t *p) {
    unsigned v=0,i; for(i=0;i<8;i++){unsigned c=p[i],d;
        if(c>='0'&&c<='9')d=c-'0'; else if(c>='a'&&c<='f')d=c-'a'+10;
        else if(c>='A'&&c<='F')d=c-'A'+10; else return 0xffffffffU;
        v=(v<<4)|d;
    } return v;
}
static uint32_t align4(uint32_t x) { return (x+3U)&~3U; }

void user_init(uint32_t mbi_phys) {
    const uint8_t *base=(const uint8_t *)(uintptr_t)mbi_phys;
    uint32_t total,off;
    module_start=module_end=0;
    if(!mbi_phys)return;
    total=*(const uint32_t *)base;
    if(total<16 || total>0x1000000U)return;
    off=8;
    while(off+8<=total){
        const struct mb2_tag *tag=(const struct mb2_tag *)(base+off);
        if(tag->size<8 || off+tag->size>total)break;
        if(tag->type==MB2_TAG_MODULE && tag->size>=16){
            const struct mb2_module *m=(const struct mb2_module *)tag;
            module_start=m->mod_start; module_end=m->mod_end; break;
        }
        if(tag->type==MB2_TAG_END)break;
        off=(off+tag->size+7U)&~7U;
    }
}

static int cpio_find(const char *wanted, struct cpio_file *out) {
    const uint8_t *p=(const uint8_t *)(uintptr_t)module_start;
    const uint8_t *end=(const uint8_t *)(uintptr_t)module_end;
    if(!module_start || module_end<=module_start)return -1;
    while(p+110<=end){
        uint32_t namesz,filesz; const char *name; const uint8_t *data;
        if(p[0]!='0'||p[1]!='7'||p[2]!='0'||p[3]!='7'||p[4]!='0'||p[5]!='1')return -1;
        namesz=hex8(p+94); filesz=hex8(p+54);
        if(namesz==0xffffffffU||filesz==0xffffffffU||namesz==0||namesz>4096)return -1;
        name=(const char *)(p+110);
        if((uint64_t)(end-(p+110))<namesz || name[namesz-1]!=0)return -1;
        data=p+((110U+namesz+3U)&~3U);
        if(data>end || filesz>(uint32_t)(end-data) || align4(filesz)>(uint32_t)(end-data))return -1;
        if(str_eq(name,"TRAILER!!!"))break;
        if(str_eq(name,wanted)){out->data=data;out->size=filesz;return 0;}
        p=data+align4(filesz);
    }
    return -1;
}

static int load_elf(const struct cpio_file *f, uint64_t *entry_out) {
    const struct elf64_ehdr *eh;
    unsigned i; int executable=0;
    uint64_t *cpt=(uint64_t *)code_pt, *spt=(uint64_t *)stack_pt;
    if(f->size<sizeof(*eh))return -1;
    eh=(const struct elf64_ehdr *)f->data;
    if(eh->ident[0]!=0x7f||eh->ident[1]!='E'||eh->ident[2]!='L'||eh->ident[3]!='F'||
       eh->ident[4]!=2||eh->ident[5]!=1||eh->ident[6]!=1||eh->type!=2||eh->machine!=62||eh->ehsize!=sizeof(*eh)||
       eh->phentsize!=sizeof(struct elf64_phdr)||eh->phnum>32)return -1;
    if(eh->phoff>f->size || (uint64_t)eh->phnum*eh->phentsize>f->size-eh->phoff)return -1;
    mem_set((void *)(uintptr_t)USER_PHYS_BASE,0,0x200000);
    mem_set((void *)(uintptr_t)USER_STACK_PHYS,0,USER_STACK_SIZE);
    mem_set(code_pt,0,PAGE_SIZE); mem_set(stack_pt,0,PAGE_SIZE);
    for(i=0;i<eh->phnum;i++){
        const struct elf64_phdr *ph=(const struct elf64_phdr *)(f->data+eh->phoff+(uint64_t)i*eh->phentsize);
        uint64_t vend,first,last,pg;
        if(ph->type!=ELF_PT_LOAD)continue;
        if(ph->filesz>ph->memsz || ph->offset>f->size || ph->filesz>f->size-ph->offset)return -1;
        if(ph->vaddr<USER_VA_BASE || ph->memsz>USER_VA_LIMIT-USER_VA_BASE)return -1;
        vend=ph->vaddr+ph->memsz;
        if(vend<ph->vaddr || vend>USER_VA_LIMIT)return -1;
        if(ph->filesz)mem_copy((void *)(uintptr_t)(USER_PHYS_BASE+ph->vaddr-USER_VA_BASE),f->data+ph->offset,(size_t)ph->filesz);
        first=(ph->vaddr-USER_VA_BASE)/PAGE_SIZE;
        last=(vend-USER_VA_BASE+PAGE_SIZE-1)/PAGE_SIZE;
        if(last>512)return -1;
        for(pg=first;pg<last;pg++){
            uint64_t flags=PTE_P|PTE_U;
            if(ph->flags&ELF_PF_W)flags|=PTE_W;
            cpt[pg]=(USER_PHYS_BASE+pg*PAGE_SIZE)|flags;
        }
        if((ph->flags&ELF_PF_X) && eh->entry>=ph->vaddr && eh->entry<vend)executable=1;
    }
    if(!executable)return -1;
    for(i=0;i<USER_STACK_SIZE/PAGE_SIZE;i++)spt[i]=(USER_STACK_PHYS+(uint64_t)i*PAGE_SIZE)|PTE_P|PTE_W|PTE_U;
    __asm__ volatile("mov %%cr3,%%rax; mov %%rax,%%cr3" ::: "rax","memory");
    *entry_out=eh->entry;
    return 0;
}

static int copy_user_string(char **sp, uint64_t *va_out, const char *s) {
    size_t n=str_len(s)+1;
    if((uintptr_t)*sp < USER_STACK_PHYS+n+16)return -1;
    *sp-=n; mem_copy(*sp,s,n);
    *va_out=USER_STACK_VA+(uint64_t)((uintptr_t)*sp-USER_STACK_PHYS);
    return 0;
}

int user_exec(const char *name,const char *args) {
    char path[96], namecopy[64], *sp; uint64_t entry,argv_va[16],user_rsp;
    uint64_t *stack_words; struct cpio_file file; unsigned argc=0,i;
    size_t n=0; int status;
    if(!name||!*name)return USER_EXEC_ERROR;
    if(name[0]=='/'){
        while(name[n]&&n+1<sizeof(path)){path[n]=name[n];n++;} if(name[n])return USER_EXEC_ERROR; path[n]=0;
        if(path[0]=='/'){
            for(i=0;i<n;i++)path[i]=path[i+1];
        }
    } else {
        const char prefix[]="bin/"; for(i=0;i<4;i++)path[i]=prefix[i];
        while(name[n]&&n+5<sizeof(path)){path[n+4]=name[n];n++;} if(name[n])return USER_EXEC_ERROR; path[n+4]=0;
    }
    if(cpio_find(path,&file)<0)return USER_EXEC_ERROR;
    if(load_elf(&file,&entry)<0)return USER_EXEC_ERROR;
    sp=(char *)(uintptr_t)(USER_STACK_PHYS+USER_STACK_SIZE);
    n=0; while(name[n]&&n+1<sizeof(namecopy)){namecopy[n]=name[n];n++;} if(name[n])return USER_EXEC_ERROR; namecopy[n]=0;
    if(copy_user_string(&sp,&argv_va[argc++],namecopy)<0)return USER_EXEC_ERROR;
    while(args && *args && argc<15){
        char token[128]; size_t k=0;
        while(*args==' '||*args=='\t')args++;
        if(!*args)break;
        while(*args && *args!=' ' && *args!='\t' && k+1<sizeof(token))token[k++]=*args++;
        token[k]=0;
        if(copy_user_string(&sp,&argv_va[argc++],token)<0)return USER_EXEC_ERROR;
        while(*args && *args!=' ' && *args!='\t')args++;
    }
    sp=(char *)((uintptr_t)sp & ~15ULL);
    if((uintptr_t)sp < USER_STACK_PHYS+(argc+2)*8)return USER_EXEC_ERROR;
    sp-= (argc+2)*8;
    stack_words=(uint64_t *)sp;
    stack_words[0]=argc;
    for(i=0;i<argc;i++)stack_words[i+1]=argv_va[i];
    stack_words[argc+1]=0;
    user_rsp=USER_STACK_VA+(uint64_t)((uintptr_t)sp-USER_STACK_PHYS);
    user_exit_status=0;
    enter_user(entry,user_rsp);
    status=user_exit_status;
    return status;
}

static int user_range(uint64_t p,uint64_t n) {
    uint64_t e=p+n, first,last,i;
    const uint64_t *pt;
    if(e<p)return 0;
    if(!n)return (p>=USER_VA_BASE && p<=USER_VA_LIMIT) || (p>=USER_STACK_VA && p<=USER_STACK_TOP);
    if(p>=USER_VA_BASE && e<=USER_VA_LIMIT) {
        pt=(const uint64_t *)code_pt; first=(p-USER_VA_BASE)/PAGE_SIZE;
        last=(e-1-USER_VA_BASE)/PAGE_SIZE;
    } else if(p>=USER_STACK_VA && e<=USER_STACK_TOP) {
        pt=(const uint64_t *)stack_pt; first=(p-USER_STACK_VA)/PAGE_SIZE;
        last=(e-1-USER_STACK_VA)/PAGE_SIZE;
    } else return 0;
    for(i=first;i<=last;i++)if((pt[i]&(PTE_P|PTE_U))!=(PTE_P|PTE_U))return 0;
    return 1;
}
long syscall_dispatch(void *raw) {
    struct syscall_frame *f=(struct syscall_frame *)raw;
    if(f->rax==ALPOS_SYS_EXIT){user_exit_status=(int)f->rdi;return EXIT_SENTINEL;}
    if(f->rax==ALPOS_SYS_WRITE){
        const char *s=(const char *)(uintptr_t)f->rsi; uint64_t i;
        extern void console_putc(char);
        if(f->rdi!=1 || f->rdx>4096 || !user_range(f->rsi,f->rdx))return -1;
        for(i=0;i<f->rdx;i++)console_putc(s[i]);
        return (long)f->rdx;
    }
    return -38;
}
