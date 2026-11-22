# corecome-out

> 一个面向 x86 / x86_64 的实验性内核与最小可引导系统，由 AI 辅助开发，目标是跑通「 bootloader → kernel → userland shell 」这条最小闭环。

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-v0.0.5-brightgreen)](https://github.com/jjjyu1940/corecome-out/releases)

---

## 这是什么

`corecome-out` 不是一个发行版，而是一个**从零搭起来的 x86 内核骨架 + 最小用户态**，用来验证以下几件事能不能真的在裸机 / 模拟器上跑起来：

- GRUB 引导一个自写的 64 位内核
- 内核里做 VGA 文本模式输出、光标管理
- PS/2 键盘轮询输入（暂不走 IRQ1）
- 跳到 userland，起一个最简 shell
- 用 `make iso` 一键产出可引导 `.iso`

项目**大量借助 ChatGPT 生成初版代码**，再由人工修 bug、调寄存器、调 Limbo 兼容性。所以你会看到 commit message 里有「由 chatGPT 接手」「二次根式?」这类痕迹——这是真实开发过程，没藏。

## 当前状态

| 维度 | 情况 |
|---|---|
| 架构 | x86_64（保留 `x86 corecome-out kernel old` 作为早期 x86 产物） |
| 引导 | GRUB / ISO9660 |
| 显示 | VGA 文本模式 80×25，光标跟随输入位置 |
| 输入 | PS/2 轮询（set-1 翻译，`0xF4` 使能扫描） |
| 用户态 | `userland/` 下有最简 shell |
| 中断 | **未接管 IRQ1**，仍为轮询模型 |
| 测试环境 | Limbo Emulator（Android）、QEMU |
| 成熟度 | 🔬 实验性，能 boot 到 shell 提示符，别拿去干正事 |

## 目录结构

```
.
├── kernel/                  # 内核源码（arch/x86_64/main.c 等）
├── userland/                # 最小用户态程序 / shell
├── build/                   # 构建中间产物
├── elf/                     # ELF 相关处理
├── iso/boot/grub/           # GRUB 配置与 ISO 布局
├── tools/                   # 辅助脚本
│   └── apply-limbo-input-fix.py
├── x86 corecome-out kernel old/   # 早期 x86 版归档
├── start.o / stub.o / stub.c      # 引导桩
├── test.elf
├── Makefile                 # 顶层构建入口
├── APPLY-TO-REPOSITORY.txt
├── BUILD-STATUS.txt
├── GITHUB-ACTIONS.md
└── LICENSE                  # AGPL-3.0
```

## 构建

依赖（Debian / Ubuntu 系）：

```bash
sudo apt install build-essential grub-common xorriso nasm
```

一键出 ISO：

```bash
git clone https://github.com/jjjyu1940/corecome-out.git
cd corecome-out
make clean
make iso
```

产物在仓库根目录，形如 `*.iso`。

### Limbo 输入修复补丁（可选）

如果你在 **Limbo Emulator** 里遇到「能显示但键盘打不进去 / 光标不动」，先打这个 overlay 再构建：

```bash
python3 tools/apply-limbo-input-fix.py
make clean
make iso
```

这个补丁做了三件事：

1. 编程 VGA CRT Controller 光标寄存器，让光标跟着 shell 输入位置走
2. 打开第一个 PS/2 端口，启用 set-1 翻译，显式发 `0xF4` 开扫描
3. **不改中断控制器**，输入仍然是轮询

> ⚠️ 该补丁基于当前 `main` 分支的 `kernel/arch/x86_64/main.c`，未在 Limbo/QEMU 上做过回归测试。打完还不行的话，抓一下 Limbo 的键盘设置截图和串口输出——有些模拟器配置对 PS/2 的暴露方式不一样。

## 运行

**Limbo（Android）**

1. 新建虚拟机，CPU 选 x86、内存 32~64MB
2. CD-ROM 挂载刚刚 `make iso` 出来的镜像
3. 启动后**点一下屏幕里的终端区拿到焦点**，再用 Limbo 的键盘按钮唤起软键盘
4. 建议装 `Hacker's Keyboard`，透明主题 + Direct Draw

**QEMU**

```bash
qemu-system-x86_64 -cdrom <你的iso> -m 64M -serial stdio
```

## 已知限制

- 没有分页 / 没有真正的进程调度，userland 是「跳过去执行」不是「系统调用切回来」
- 键盘是轮询，占 CPU，长按会刷
- 没有文件系统，没有磁盘驱动
- Limbo 下软键盘映射因机型而异，部分设备需要换 IME
- commit 历史比较野，建议看 tag（`v0.0.5` 及之前）而不是 `main` 最新一条

## 路线图

- [ ] IRQ1 中断式键盘，替掉轮询
- [ ] 简易 IDT/PIC 完整初始化
- [ ] userland 通过 syscall 回内核（而不是裸跳转）
- [ ] 串口日志分级（`BUILD-STATUS.txt` 目前是手写状态）
- [ ] GitHub Actions 自动出 ISO（骨架已在 `.github/workflows`，见 `GITHUB-ACTIONS.md`）

## 贡献

欢迎 issue 和 PR，尤其是：

- 在真机 / 其他模拟器（VirtualBox、Bochs、SeaBIOS）上跑通的反馈
- PS/2、VGA、GRUB 配置相关的修复
- 把「AI 生成但没验证」的代码段挑出来标 TODO

提交前跑一遍：

```bash
make clean && make iso
```

确保能出盘再提。

## 许可

**AGPL-3.0** —— 详见 [LICENSE](LICENSE)。
衍生项目（包括以服务形式对外提供）都需要开源，别绕。

---

<sub>本项目由 AI 辅助生成初版代码，人工调试维护。能跑就行，别生产。</sub>
