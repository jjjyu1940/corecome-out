# GitHub Actions build

1. Apply the overlay from the repository root:
   `bash alpos-gui-stage1/tools/apply-gui-stage1.sh .`
2. Commit and push the resulting source changes to `main` (or open a pull request targeting `main`).
3. Open GitHub → Actions → **alpOS x86_64 build**.
4. Wait for the **Build bootable ISO** job to finish successfully.
5. Download `alpos-x86_64-iso-<commit>` from the run's Artifacts section.

The artifact contains the bootable ISO, kernel ELF and userland initramfs. Build outputs are not committed to the Git repository.
