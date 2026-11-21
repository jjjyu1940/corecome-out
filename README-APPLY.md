# Apply alpOS userspace Stage 2

Copy `userland/`, `tools/mknewc.py`, and `docs/STAGE2-ABI.md` into the repository root.

Build and validate the standalone ELF64 programs:

```sh
make -C userland CROSS=x86_64-linux-gnu- clean verify initramfs
```

This stage does not change the existing kernel or claim that user programs are runnable. Ring 3, syscall dispatch, ELF loading, argv setup, and initramfs handoff are the next kernel tasks.
