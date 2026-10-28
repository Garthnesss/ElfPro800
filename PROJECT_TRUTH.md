# ElfChords P8 — Project Truth

## One line
One portable C99 8-voice polysynth core (from GliGli's P600fw logic) that ships first as a VST3/AU/CLAP + standalone plugin, and later as replacement firmware for the Behringer PRO-800.

## What it is
- A dual-target project under the **ElfChords** brand, author **Garth Elfstrom**. Working title `elfchords-p8` (final product name is Garth's call, still open).
- The core outputs a virtual **CvFrame** per control tick; the plugin turns it into audio via an analog-voice DSP model, the firmware turns it into DAC/S&H writes on the real hardware. Same core, same patches, same behaviour.
- Synth logic is derived from **GliGli's P600fw** (Prophet-600 replacement firmware, GPL-3.0, `common/` ≈ 7.6k lines — verified on this VM 2026-10-28: 7,583 lines of `.c`). Hardware-coupled P600fw files (`dac, display, potmux, scanner, sh, tuner, uart_6850`) are replaced, not ported.
- The PRO-800 target is officially sanctioned for custom firmware: Music Tribe published `PRO800-Firmware-Packer` (HEAD `c845054`, 2026-10-23 — verified) with the main-PCB schematic and a closed binary packer. MCU STM32F446, app linked at `0x08010000`, max 128 kB, bootloader write-protected.

## What it is not
- Not a clone/skin of the stock PRO-800 firmware — that source is not public and this project does not contain any of it.
- Not a hardware project yet: Garth does **not** own a PRO-800. Phases H0+ do not start until he buys one and says go.
- Not affiliated with Behringer / Music Tribe / Sequential / GliGli. Those names never appear in product names, plugin names, bundle IDs, logos or class names (nominative description only, e.g. "custom firmware for the Behringer PRO-800").

## Non-negotiables
1. **GPL-3.0-or-later for the whole repo.** P600fw code is GPL-3.0; P600fw-derived code stays isolated in `core/p600fw_derived/` so a future closed-source product would require a clean-room core swap, never a licence shortcut.
2. **Never unlock flash read-protection via JTAG/SWD on the PRO-800** — it mass-erases the bootloader and bricks the unit. Firmware flashing is bootloader-SysEx only.
3. **Never invent hardware facts.** Anything unverified stays behind a config constant with a TODO and an entry in `docs/UNVERIFIED.md` until confirmed on real hardware.
4. **Small verifiable steps.** Every step ends with a build/test run whose output is shown; nothing is claimed working without evidence.

## Confirmed by Garth
- 2026-10-28: Handed over the roadmap + coding prompt (`docs/input/`) defining this project: VST-first, firmware-second; start with V0 only; licence/framework choice, repo tree and first 5 commits to be proposed before code is written.

## Scope boundaries
- In scope now: V0 (host core + tests + render CLI). V1 plugin and V2 features follow in order.
- Out of scope until a unit is bought and Garth says go: all H-phase hardware work, any purchase, publishing anything, final product name.
