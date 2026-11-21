/*
 * start.S —— EL0 程序的 ELF 入口。
 *
 * 内核 proc_exec_path 把 argc 放进 x0、argv 指针放进 x1（约定见 proc.c）。
 * 我们自己构造栈上的 argv/environ 并把 environ 设好，再跳进 main()。
 *
 * 栈布局（高地址往下）：
 *   sp+0     : argc
 *   sp+8..   : argv[0..argc-1]，后跟 NULL
 *   ...      : 各 argv 字符串本体
 *   ...      : envp 数组（当前留空，只有 NULL）
 *
 * 因内核只传 argc/argv，这里不构造完整 auxv，只把 environ 指向一个 NULL。
 */

    .arch armv8-a
    .section .text.start, "ax"
    .global _start
    .align 4

_start:
    /* 1) 准备栈：16 字节对齐，留出 256 字节应急空间（内核给的栈就在用户窗顶） */
    mov  x9, sp
    and  x9, x9, #~15
    mov  sp, x9
    sub  sp, sp, #256

    /* 2) 把内核给的 argc/argv（在 x0/x1）搬到当前栈上，建 environ=NULL */
    stp  x0, x1, [sp, #-16]!     /* 暂存：argc, argv */
    mov  x0, #0
    adrp x1, environ
    str  x0, [x1, #:lo12:environ]   /* environ = &empty_env */

    ldp  x0, x1, [sp], #16        /* 恢复 argc, argv */
    bl   main                     /* main(argc, argv) */

    /* 3) main 返回 -> exit(code) */
    mov  w0, w0
    bl   sys_exit

    /* 永远不回来，但以防万一 */
    b    .
