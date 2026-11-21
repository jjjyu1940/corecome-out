# alpOS x86_64 build overlay

This package is an overlay for the existing `jjjyu1940/corecome-out` `main` branch, not a full clone of the repository. Copy the included `Makefile` to the repository root and `.github/workflows/alpos-x86-iso.yml` to the matching workflow path, replacing the old workflow.

## Commands after applying the overlay

- `make` / `make x86-build`: compile `kernel/build-x86/alpos-kernel.elf`.
- `make iso`: build and validate `kernel/build-x86/alpos.iso`.
- `make test`: build the ISO and run the QEMU serial boot smoke test.
- `make run`: launch an existing ISO in QEMU with an interactive serial console.
- `make clean`: remove x86_64 build output.

Ubuntu 24.04 dependencies:

```sh
sudo apt-get update
sudo apt-get install -y make gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  grub-pc-bin grub-efi-amd64-bin grub-common xorriso qemu-system-x86
```

GitHub Actions runs on pushes to `main`, relevant pull requests, and manual dispatch. It explicitly runs `make`, then the required `make iso` step, checks that `kernel/build-x86/alpos.iso` is non-empty, and uploads the ELF and ISO as the `alpos-x86_64-elf-and-iso` artifact. QEMU smoke testing is separate and non-blocking, so a QEMU-only failure will not suppress an already-created ISO.

The included `build/alpos-kernel.elf` is a locally built ELF64 x86_64 kernel. It is not an ISO. The local environment does not have GRUB rescue tooling, xorriso, or QEMU, so ISO creation and boot testing are delegated to the GitHub Actions runner and are not claimed as locally verified.

This remains an x86_64 bring-up milestone, not a finished general-purpose operating system.
