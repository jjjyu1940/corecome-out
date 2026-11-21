# tools/ci/Dockerfile —— alpOS 可复现构建环境。
#
# 镜像职责：装齐 x86_64 交叉工具链与 ISO 打包所需的一切，把“在我机器上能编”
# 变成“在任意 CI runner 上能编”。基础镜像锁定 ubuntu-24.04 + GCC 13，与
# 上游仓库 .github/workflows/kernel.yml 的基线保持一致（该基线明确注明
# 新版 QEMU 存在无串口问题，不升级）。
#
# 分层设计：
#   base     系统包与构建工具，变化少，缓存命中率高；
#   toolchain  交叉编译器与模拟器，体积大，单独一层；
#   verify    用 scons 之外的手段做静态校验，不涉及运行时。

FROM ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

RUN apt-get update -qq \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl wget git \
        build-essential make autoconf \
        python3 python3-pip \
        file binutils \
        xxd \
        qemu-system-x86 qemu-utils ovmf \
        xorriso grub-pc-bin grub-efi-amd64-bin \
        gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
        gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
        mtools dosfstools \
        e2fsprogs \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# --- toolchain ----------------------------------------------------------
# 验证工具链可用性，输出版本供构建日志留存（出问题时能定位“哪一天”的镜像）。
RUN set -eux; \
    x86_64-linux-gnu-gcc --version | head -1; \
    aarch64-linux-gnu-gcc --version | head -1; \
    xorriso --version 2>/dev/null | head -1 || true; \
    qemu-system-x86_64 --version | head -1

# --- verify -------------------------------------------------------------
# 做静态校验，不运行内核：读取 ELF 段、解析 GRUB 配置、检查 multiboot2 头。
COPY tools/ci/verify_iso.py /usr/local/bin/verify_iso.py
RUN chmod +x /usr/local/bin/verify_iso.py

CMD ["bash", "-c", "make -C kernel x86-build && bash tools/image/mkiso.sh && verify_iso.py kernel/build/alpos-latest.iso"]
