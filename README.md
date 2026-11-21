

# alpOS — 从零开始的 x86_64 教学操作系统

alpOS 是一个**教学级 x86_64 宏内核**，从 Multiboot2 引导到用户态 ELF 加载全部手写。  
它的目标不是取代 Linux，而是让你能在 **一个周末内理解现代操作系统的核心骨架**。

## 它有什么用？

- **学习 x86_64 长模式、分页、中断、系统调用**：所有代码都在 `kernel/arch/x86_64/` 下，清晰分离架构层。
- **动手实验平台**：你能在 QEMU 里跑起来，然后用 `printf` 调试每一个硬件细节。
- **可扩展的起点**：已经有了 COW fork、信号、ramfs、ext2 只读支持（AArch64 侧已验证），x86_64 侧正在移植。
- **GitHub Actions 一键构建 ISO**：提交代码，几分钟后就能拿到可启动光盘镜像。

## 快速开始

### 环境要求
- x86_64 Linux 主机（或 WSL2）
- `gcc`、`ld`、`make`、`grub-pc-bin`、`xorriso`、`qemu-system-x86_64`

### 构建并运行

bash

make -f Makefile.x86 iso        # 编译内核并打包 ISO

qemu-system-x86_64 -cdrom build/alpos.iso -serial stdio


### 只用 GitHub Actions（无需本地环境）
1. Fork 本仓库
2. 进入 Actions 页面，选择 `Build alpOS ISO`
3. 点击 Run workflow → 等待几分钟 → 下载 Artifact 中的 `alpos.iso`

## 当前状态

| 组件 | 状态 |
|------|------|
| Multiboot2 引导 | ✅ |
| 长模式切换 (64-bit) | ✅ |
| GDT / IDT / TSS | ✅ |
| 8259A PIC | ✅ |
| 串口 + VGA 控制台 | ✅ |
| 四级分页 (MMU) | 🟡 桩实现，缺页会 panic |
| 系统调用 (syscall) | ✅ 框架，18 个调用 |
| 用户态切换 (ring 3) | 🟡 桩实现 |
| 进程调度 | 🟡 来自 AArch64 侧，需适配 |
| 文件系统 (ramfs/ext2) | 🟡 来自 AArch64 侧，需适配 |
| ISO 打包 | ✅ |
| GitHub Actions CI | ✅ |

🟡 = 已移植但未充分测试，欢迎提 Issue 或 PR。

## 项目结构

alpOS/

├── kernel/

│   ├── arch/x86_64/       # x86_64 架构层（引导、GDT、IDT、分页、控制台……）

│   ├── core/              # 内核核心（system.c、panic、运行时信息）

│   ├── mm/                # 内存管理（页分配器、slab）

│   ├── process/           # 进程管理（TCB、调度器）

│   ├── syscall/           # 系统调用表

│   └── vfs/               # 虚拟文件系统（ramfs、ext2）

├── iso/                   # ISO 打包文件（grub.cfg）

├── tools/                 # 构建脚本、CI 配置

├── Makefile.x86           # x86_64 构建入口

└── .github/workflows/     # GitHub Actions 自动构建


## 许可证

AGPL-3.0-only — 详见 [LICENSE](LICENSE)。

## 致谢

- 灵感来源于 [corecome-out](https://github.com/110we/corecome-out) 项目
- 感谢所有在 AI 辅助下写出可运行内核的人类与机器

