# ElfChords P8 — Architecture

## Big picture
```
                    ┌──────────────────────── core/ (C99, no malloc) ───────────────────────┐
  pots/switches/    │  assigner · adsr · lfo · glide · arp · seq · clock · midi · storage   │
  encoder/buttons ──┤►  patch model · ui state machine                                      ├──► CvFrame
  MIDI bytes/sync   │  (logic derived from P600fw, isolated in core/p600fw_derived/)        │    per tick
                    └────────────────────────────── hal.h ──────────────────────────────────┘
                                          ▲                                   │
                          ┌───────────────┴────────────┐        ┌─────────────▼─────────────┐
                          │ targets/plugin (V1)        │        │ targets/pro800 (H1)       │
                          │ CvFrame → DSP voice model  │        │ CvFrame → DAC80501 + 4051 │
                          │ → VST3/AU/CLAP/standalone  │        │ S&H refresh on STM32F446  │
                          └────────────────────────────┘        └───────────────────────────┘
```

## Components
- **core/** — portable C99, deterministic, fixed control rate (start 2 kHz, a named constant). No globals visible outside. The only interface to a target is `hal.h`.
- **core/p600fw_derived/** — imported P600fw logic files with original GPL headers intact, plus the portability shims (`PROGMEM`, `pgm_read_byte`, `CYCLE_WAIT`, `BLOCK_INT`, `MDELAY`, `print.h` stubs, host `itoa`). Isolated so it can be swapped for a clean-room core later.
- **hal.h contract** — Inputs: `pots[26]` (uint16), switches bitfield, encoder delta, panel buttons bitfield, MIDI byte stream, sync ticks, tuner capture, storage page read. Outputs: `CvFrame` = `oscA[8], oscB[8], fil[8], amp[8]` (uint16) + globals (`pwA, pwB, volA, volB, res, noise, mvol, pmodB, extFil`) + per-voice/global digital bits (waveforms, sync, poly-mod, unison, keytrack) + display text (4 chars) + LED bits + MIDI out + storage writes.
- **targets/plugin/** (V1) — JUCE 8 under AGPL-3.0 (+ clap-juce-extensions for CLAP), VST3 SDK ≥3.8 (MIT). `dsp/` holds the analog voice model: 2 VCOs (saw/tri/pulse, hard sync, poly-mod), noise, 24 dB/oct resonant LPF, VCA, per-voice tolerance/drift, CV slew mimicking S&H refresh. No allocation in the audio thread.
- **targets/pro800/** (H-phase, not started) — bare-metal STM32F446: CMSIS + LL/register level, TinyUSB (MIT) for USB-MIDI, linker script with FLASH ORIGIN `0x08010000` LENGTH 128K, RAM 128K; startup sets `SCB->VTOR = 0x08010000` and fully re-initialises clocks/IRQs (bootloader hand-off state unverified).
- **tools/** — `render` CLI (MIDI file + patch → CvFrame CSV log → placeholder DSP → 48 kHz WAV), SysEx patch tools.
- **tests/** — host unit tests (plain C asserts via CTest) + golden CvFrame snapshot tests shared by both targets.

## Data flow / protocols
- Control path: target HAL samples inputs → core tick → CvFrame → target renders it (audio DSP or DAC/S&H).
- PRO-800 SysEx: header `F0 00 20 32 00 01 24 00`; update-mode entry `… 03 30 F7`; packer output = one header message (`…24 00 0A`, 66 B, contains `P0E9I` + product string + version) then 255-byte data messages (`…24 00 0B 00 <block#>`); payload encrypted by the official tool — never re-implemented. Patch import/export uses the community-documented format (samstaton/pro800): request `77 xx yy`, response `78 xx yy …`, 8-bit data packed in 7-bit groups, fields from decoded offset 5, name at index 150. Treated as partially unverified; import must be tolerant.
- Firmware update rate ≤ 9.64 kB/s over SysEx; recovery = hold buttons 0+7 at power-up → SynthTribe stock reinstall.

## Dependencies
- **P600fw** (GPL-3.0) — logic base. Snapshot in `references/p600fw/` (upstream pushed 2026-04-13).
- **PRO800-Firmware-Packer** (repo AGPL-3.0; tool used as-is, binary-only) — schematic + `PRO800_update_gen` for Linux/macOS/Windows. Snapshot in `references/PRO800-Firmware-Packer/` at HEAD `c845054`.
- **JUCE 8** (AGPL-3.0 for this open-source project), **VST3 SDK ≥3.8** (MIT), **TinyUSB** (MIT), **STM32 CMSIS/LL** (Apache-2.0 / BSD-3) — firmware phase only.
- Community docs: samstaton/pro800 (SysEx + bug/wishlist lists), lsim/pro800cc (CC map, MIT).

## What's missing
- Voice-board schematic (not released by Music Tribe) — CV scaling is settled empirically via auto-tune in H1.
- Stock PRO-800 firmware source, SDK, HAL, example firmware — none exist publicly; the packer repo is all there is.
- A PRO-800 unit — none owned yet; all hardware facts beyond the README/schematic stay in `docs/UNVERIFIED.md`.
