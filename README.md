# alpOS — corecome-out Kernel Prototype / alpOS 内核原型系统
This is based on https://github.com/110we/corecome-out The developed operating system. Right now, he's just a circle.This is just a prototype.
**English | [简体中文](#简体中文)**

> alpOS is the first prototype system built on the `corecome-out` kernel project. It is an experimental platform for people who want to learn about and collaboratively develop operating systems. **It is not a usable, production-ready operating system or a Linux distribution.**

## Project purpose

The project provides a small bootable starting point: bootloader, x86_64 kernel, basic console/graphical framebuffer output, early keyboard input, and a minimal userland. The intention is to let contributors modify, replace, and extend system components, much like communities build different distributions (for example Debian) on top of the Linux kernel. alpOS itself is a prototype base, not a finished distribution.

## AI use and AI-assisted development

**AI was used in this project.** ChatGPT and other AI-assisted tools may be used to draft code, explain architecture, propose patches, generate tests and documentation, and help diagnose build or boot failures. Human contributors must review generated changes, run builds and emulator tests, and clearly report unverified behavior. AI-generated code is not automatically correct; do not treat an AI suggestion or a successful compile as proof that a feature works.

Suggested workflow for developing with AI:
1. Clone the repository and identify the exact source files and current build instructions.
2. Ask an AI assistant to explain a small subsystem (for example boot, framebuffer, keyboard, memory, or filesystem) before requesting changes.
3. Give the assistant the relevant source files, compiler output, serial log, and emulator configuration. Never rely on an invented repository layout.
4. Request a small, reviewable patch with a list of changed files, assumptions, and known limitations.
5. Build locally or with GitHub Actions; run QEMU/Limbo tests where possible.
6. Inspect the diff, keep working backups, document what was and was not tested, then submit a pull request.
7. Iterate: use test failures and logs as evidence, not guesses.

AI is a development aid, not a substitute for understanding, review, testing, or maintainer responsibility. Do not paste secrets, access tokens, private keys, or personal data into AI tools.

## Current prototype status

- Target: x86_64, booted through GRUB/Multiboot2.
- First interface: full-screen text terminal drawn on the framebuffer when a supported framebuffer is provided; VGA text fallback remains.
- Input: early PS/2 polling and optional serial input path; device behavior depends on emulator configuration.
- Userland: experimental ELF64 loading/syscall work may be present in the current branch.
- Storage: no claim of a complete persistent disk/filesystem implementation.
- Maturity: educational and experimental only. Expect missing features, bugs, regressions, and incompatible hardware.

## Build locally

On Ubuntu/Debian, install the toolchain (package names can vary by release):

```sh
sudo apt-get update
sudo apt-get install -y build-essential gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  grub-pc-bin grub-common xorriso mtools python3
make clean
make iso
```

The expected ISO path for the current x86 build is `kernel/build-x86/alpos.iso`.

## GitHub Actions build

The workflow in `.github/workflows/alpos-build.yml` runs on pushes to `main`/`master`, pull requests, and manual dispatch. It installs the cross compiler and ISO tools, builds the kernel and initramfs, runs the repository's smoke checks, and uploads the ISO and ELF/initramfs outputs as an Actions artifact. Open the repository's **Actions** tab, select the `alpOS x86_64 build` workflow, open a successful run, and download `alpos-x86_64-gui-iso` under **Artifacts**. A green workflow means the configured build/check steps passed; it does not prove all hardware features work.

## Run in QEMU

```sh
qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso -serial stdio
```

For Limbo, create an x86_64 virtual machine and attach the ISO as its CD-ROM. Keyboard/mouse delivery depends on the selected machine and input-device settings.

## Contributing

This repository is intended as a shared operating-system development prototype. Contributions are welcome in boot, console, input, memory management, drivers, filesystems, userland, build tooling, tests, and documentation. Prefer small pull requests with reproducible build/test logs and explicit limitations.

## Disclaimer

**Not a usable operating system. Not for production or important data.** Features may be incomplete, non-persistent, unsafe, or untested. Back up your work and use disposable virtual disks.

## License

See [`LICENSE`](LICENSE).

---

# 简体中文

> alpOS 是基于 `corecome-out` 内核项目开发的**第一个原型系统**。它是一个供操作系统爱好者学习、实验和协作开发的起点。**它不是可实际使用的成熟操作系统，也不是可用于生产环境的系统或 Linux 发行版。**

## 项目定位

本项目提供一个最小可启动基础，包括引导流程、x86_64 内核、基础控制台/Framebuffer 图形输出、早期键盘输入和最小用户态。项目希望让开发者能够共同修改、替换和扩展系统组件。它的协作模式可以类比为：不同社区基于 Linux 内核开发不同发行版，例如 Debian；不同开发者也可以基于本原型继续构建自己的操作系统。alpOS 是原型底座，不是已经完成的发行版。

## AI 使用声明与 AI 辅助开发方法

**本项目明确使用了 AI 辅助开发。** 项目开发过程中使用 ChatGPT 等 AI 工具辅助编写初版代码、解释系统架构、提出补丁、生成测试与文档，以及分析编译错误和启动日志。所有 AI 生成或建议的代码都需要开发者审查，并通过实际编译和模拟器测试验证。AI 输出不保证正确；编译成功也不代表功能已经能够正常工作。

推荐的 AI 辅助开发流程：
1. 克隆仓库，先确认当前分支、真实目录结构和构建方法。
2. 先让 AI 解释一个具体子系统，例如引导、Framebuffer、键盘、内存管理或文件系统，再提出修改要求。
3. 向 AI 提供相关源码、完整编译输出、串口日志和模拟器配置；不要让 AI 猜测不存在的文件或接口。
4. 要求 AI 提交范围小、便于审查的补丁，并列出修改文件、设计假设和已知限制。
5. 使用本地环境或 GitHub Actions 编译；条件允许时再用 QEMU/Limbo 实际启动测试。
6. 检查代码差异，保留备份，记录哪些功能已经验证、哪些尚未验证，然后再提交 Pull Request。
7. 根据真实测试日志持续迭代，不要用猜测代替证据。

AI 是开发辅助工具，不能代替开发者理解代码、审查、测试和承担维护责任。不要向 AI 工具提交密码、访问令牌、私钥或个人敏感信息。

## 当前原型状态

- 目标架构：x86_64，通过 GRUB/Multiboot2 引导。
- 首个界面：在引导器提供受支持的 framebuffer 时绘制全屏终端；同时保留 VGA 文本回退。
- 输入：早期 PS/2 轮询，并保留可选串口输入路径；设备是否可用取决于模拟器配置。
- 用户态：当前分支可能包含实验性的 ELF64 加载和系统调用代码。
- 存储：目前不宣称已经具备完整、可靠、可持久化的硬盘和文件系统实现。
- 成熟度：仅供学习和实验，可能存在缺失功能、错误、回归问题及硬件兼容性问题。

## 本地编译

Ubuntu/Debian 可安装以下工具（不同发行版的包名可能略有差异）：

```sh
sudo apt-get update
sudo apt-get install -y build-essential gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  grub-pc-bin grub-common xorriso mtools python3
make clean
make iso
```

当前 x86 构建预期生成：`kernel/build-x86/alpos.iso`。

## GitHub Actions 自动编译

`.github/workflows/alpos-build.yml` 工作流会在推送到 `main`/`master`、Pull Request 或手动触发时运行。它会安装交叉编译器和 ISO 工具，编译内核及 initramfs，执行仓库配置的检查，并将 ISO、ELF 和 initramfs 上传为 Actions Artifact。打开仓库的 **Actions** 页面，选择 `alpOS x86_64 build`，进入成功的运行记录，在 **Artifacts** 下载 `alpos-x86_64-gui-iso`。工作流变绿只表示配置的构建与检查步骤通过，不代表所有硬件功能均已验证。

## 使用 QEMU 运行

```sh
qemu-system-x86_64 -machine pc -m 256M -cdrom kernel/build-x86/alpos.iso -serial stdio
```

使用 Limbo 时，创建 x86_64 虚拟机并将 ISO 挂载为 CD-ROM。键盘和鼠标能否传入取决于虚拟机机型及输入设备设置。

## 参与开发

本仓库定位为一个开放的操作系统原型开发项目。欢迎围绕引导、控制台、输入、内存管理、驱动、文件系统、用户态、构建工具、测试和文档提交贡献。建议每个 Pull Request 尽量小，并附上可复现的编译/测试日志和明确的功能限制。

## 免责声明

**这不是一个可用的操作系统，也不适合生产环境或存放重要数据。** 功能可能不完整、不持久、不安全或未经测试。请备份工作内容，并使用可随时丢弃的虚拟硬盘。

## 许可证

请参阅 [`LICENSE`](LICENSE)。
This is a Versions based on changes from other kernel projects. The original project is in.https://github.com/110we/corecome-out
We used the same license to ensure its compliance.
Thank 110we for his development.
It should be noted that we have used here It is part of its architecture, and then it is transferred and ported to the x86 platform using AI.