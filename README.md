# alpOS Limbo keyboard + cursor fix

This is a small overlay for the current `main` branch. It does not replace the kernel or merge anything into GitHub.

## Apply

From the repository root, run:

```sh
python3 tools/apply-limbo-input-fix.py
make clean
make iso
```

Then boot the rebuilt ISO in Limbo Emulator. Tap the terminal display to focus it, then use Limbo's keyboard button to open the Android soft keyboard.

## Changes

- Programs the VGA CRT controller cursor registers so the text cursor follows the shell input position.
- Enables the first PS/2 port, enables set-1 translation, and explicitly sends keyboard command `0xF4` to enable scanning.
- Keeps keyboard input polling-based; it does not enable IRQ1 or change the interrupt controller setup.

## Notes

This patch is based on the `kernel/arch/x86_64/main.c` currently present on `main`. It has not been tested in Limbo/QEMU here. If typing still fails after rebuilding, capture the Limbo keyboard settings and serial output; some emulator configurations may expose input differently.
