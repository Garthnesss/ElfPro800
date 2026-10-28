# Prompt for the coding agent — paste everything below this line

You are a senior embedded + audio-plugin engineer. Build a **dual-target 8-voice analog-polysynth project** for the brand **ElfChords** (author **Garth Elfstrom**): one **portable synth core** in C99, shipped first as a **VST3/AU/CLAP + standalone plugin** (start now, no hardware needed), and later as **replacement firmware for the Behringer PRO-800** (I don't own the unit yet). Work in small, verifiable steps; after every step run the build/tests and show me the result before moving on. Never invent hardware facts: anything marked **[UNVERIFIED]** below must stay behind a config constant with a TODO until confirmed on real hardware.

## 0. Ground rules
- Project working title: `elfchords-p8` (placeholder; I'll pick the final name). **Never** use "Behringer", "PRO-800", "Prophet", "Sequential", "SynthTribe", "Music Tribe" or "GliGli" in product names, plugin names, bundle IDs, logos or class names. Descriptions may say "custom firmware for the Behringer PRO-800" (nominative use). Plugin manufacturer: `ElfChords`, manufacturer code `Elfc`, author `Garth Elfstrom`.
- **License decision (do this first, ask me if unsure):** Default = **open source GPL-3.0-or-later** for the whole repo, because we reuse GliGli's P600fw (GPL-3.0). With that choice: use **JUCE under AGPL-3.0** (compatible with GPL-3.0 via §13) *or* a permissive framework (DPF/ISC, iPlug2) — your pick, justify it. VST3 SDK ≥3.8 is MIT. Keep all original copyright headers, add a `CREDITS.md` crediting GliGli, Imogen/image-et-son and P600fw contributors. If I later want a closed-source commercial plugin, the core must be a **clean-room rewrite with no P600fw code** — so keep P600fw-derived code isolated in `core/p600fw_derived/` to make that swap possible.
- If you use the VST name/logo, follow Steinberg's usage guidelines and include "VST is a registered trademark of Steinberg Media Technologies GmbH."
- Small commits, conventional messages, CI from day one (GitHub Actions: Linux + macOS + Windows).

## 1. References (read these first)
1. GliGli P600fw (GPL-3.0, Prophet-600 replacement firmware, C): https://github.com/gligli/p600fw — the PRO-800 was developed by Behringer together with GliGli and ships "the iconic GliGli modification as standard" (Behringer launch text, 2022-12-07). The PRO-800's own firmware source is **not** public.
   - `common/` (~7.6k lines) contains the synth logic: `synth.c, assigner.c, adsr.c, lfo.c, arp.c, seq.c, clock.c, midi.c, storage.c, ui.c, utils.c` (+ `xnormidi/`). Verified: with a tiny shim they compile for both host gcc and arm-none-eabi. The shim needed: `#define PROGMEM`, `#define pgm_read_byte(p) (*(const uint8_t*)(p))`, empty `CYCLE_WAIT(c)`, `BLOCK_INT`, `MDELAY(ms)`, a `print.h` with empty `print/phex/phex16/phex32` macros, and a replacement for the non-standard `itoa` used in `synth.c` (host only).
   - Hardware-coupled files to **replace, not port**: `dac.c, display.c, potmux.c, scanner.c, sh.c, tuner.c, uart_6850.c` and the 3 `mem_write/io_write` call sites in `synth.c` (they target the 1982 Z80 I/O map). `common/hardware.h` is the original abstraction point ("implemented by firmware or mockup").
2. PRO-800 firmware packer (official, 2026-10-22): https://github.com/MusicTribeOpenSource/PRO800-Firmware-Packer — README + closed binary `PRO800_update_gen` (Linux x86-64, macOS arm64, Windows) + main-PCB schematic PDF (5 sheets). No SDK, no example firmware, no voice-board schematic.
3. Community PRO-800 SysEx docs: https://github.com/samstaton/pro800 (`pro800syx.md`, `defros/`), CC map https://github.com/lsim/pro800cc.

## 2. Architecture you must build
```
elfchords-p8/
  core/                 C99, no malloc, no globals visible outside, deterministic, fixed control rate (start with 2 kHz; make it a constant)
    p600fw_derived/     imported P600fw logic (GPL headers intact) + shims
    hal.h               the ONLY interface between core and a target
  targets/plugin/       VST3 + AU + CLAP + standalone
    dsp/                analog voice model
  targets/pro800/       STM32F446 bare-metal firmware (later)
  tools/                render CLI, sysex tools
  tests/                unit + golden tests shared by both targets
```
`hal.h` contract (adapt names, keep the split):
- Inputs to core: `pots[26]` (uint16 0..65535), `switches` bitfield, `encoderDelta` (int), panel `buttons` (bitfield, includes number buttons 0–9 and PRESET/PERF/SETTINGS/TUNE/SYNC SOURCE/SYNC CLOCK/REC/SEQ1/SEQ2/ARP UP-ON/ARP-ASSIGN), MIDI byte stream in, external sync ticks, tuner capture values, storage page read.
- Outputs from core: a **`CvFrame`** per control tick = `oscA[8], oscB[8], fil[8], amp[8]` (uint16) + globals `pwA, pwB, volA, volB, res, noise, mvol, pmodB, extFil` + per-voice/global digital bits (saw/tri/sqr A & B, sync, polymod-to-freqA, polymod-to-filter, unison, keytrack off/half/full) + display text (4 chars) + LED bits + MIDI bytes out + storage page writes.
- The plugin turns `CvFrame` into audio; the firmware turns it into DAC/S&H writes. Same core, same patches.

## 3. Milestones (do them in order; each has acceptance criteria)

### V0 — core builds and is tested on the host (today)
- CMake project; import P600fw `common/` logic files into `core/p600fw_derived/`; write the shims; implement a `hal_mock` that records CvFrames.
- Unit tests (Catch2/doctest or plain C asserts via CTest): envelope timing/shape, LFO waveforms & rate, voice assigner (8 voices, priority modes, unison), glide, arp up/down/assign, sequencer record/playback, MIDI clock sync at 24 PPQN, MIDI parser incl. running status and SysEx.
- `tools/render`: input a .mid file + patch → CvFrame log (CSV) → simple placeholder DSP → 48 kHz WAV.
- Also cross-compile `core/` with `arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Os` in CI.
- **Accept when:** `cmake --build && ctest` green on 3 OSes; ARM core build green; `render` produces an audible 8-voice chord WAV; a golden-CvFrame snapshot test exists.

### V1 — playable plugin
- Plugin: VST3 + AU (macOS) + CLAP + standalone. Parameters for every panel control (26 pots: amp ADSR, filter ADSR, filter env amount, cutoff, resonance, freq A, freq B, fine B, A level, B level, A PW, B PW, noise, glide, LFO freq, LFO initial amount, poly-mod filter env amount, poly-mod osc B amount, master tune, master volume, …; switches: saw/tri/sqr A & B, sync, polymod→freqA, polymod→filter, LFO shape/target, keytrack off/half/full, unison) and the VALUE encoder/menu.
- DSP voice: 2 VCOs (saw, tri, variable pulse; hard sync B→A; poly-mod from filter env and osc B into osc A pitch and filter), noise, 24 dB/oct resonant low-pass, VCA, per-voice component tolerance and slow drift, CV slew to mimic S&H refresh. Oversample where aliasing matters. No allocation in the audio thread.
- 400-patch bank (4×100), DAW state save/restore, MIDI CC map compatible with the PRO-800 (use lsim/pro800cc + samstaton docs; mark unconfirmed CCs).
- PRO-800 SysEx patch import/export using the community format: header `F0 00 20 32 00 01 24 00`, patch request `77 xx yy` (xx = LSB of 0–399), response `78 xx yy <data>`; data is 8-bit packed in 7-bit groups where a leading byte holds the MSBs of the next seven (LSB of that byte → MSB of byte 1, …, bit 6 → MSB of byte 7); fields from decoded offset 5 (Freq A, Vol A, PW A, Freq B, …) per `pro800syx.md`; name at index 150 (0x96), NUL-terminated. Treat the table as **[UNVERIFIED beyond what the doc states]** and make import tolerant.
- **Accept when:** passes `pluginval --strictness-level 10` and Steinberg `validator`; loads and plays in Reaper (and Logic/Ableton if available); a patch file from `samstaton/pro800` examples (or a hand-built test vector) round-trips; CPU < 5 % of one core at 48 kHz/128 samples with 8 voices on a modern laptop.

### V2 — ElfChords features (implemented in core so they later run on hardware)
In priority order, each behind a patch/system setting, each with tests:
1. LFO tempo-sync to MIDI clock (and to internal arp/seq clock).
2. Mod wheel → VCF routing (plus amount).
3. Sequencer SysEx dump/load and more stored sequences.
4. Latched transpose mode for seq/arp.
5. Small mod matrix (sources: LFO, filter env, amp env, velocity, aftertouch, mod wheel, osc B, random; destinations: freq A/B, PW A/B, cutoff, resonance, VCA, LFO rate/amount) within control-rate budget.
6. Randomize patch triggered by CC/note/sequencer step.
7. Two-unit chaining (voice overflow to MIDI out for 16 voices).
8. Avoid the stock-firmware bugs listed by users (Arp-Mode CC#49 not transmitted; Subdivision on CC#50 collides with OSC A Freq LSB — use a free CC; per-voice manual retune not persisted; patch name lost on save-as; pedal polarity missing from system dump).
- **Accept when:** each feature has a unit test + a demo preset + a line in `CHANGELOG.md`.

### H0 — hardware hello world (only after I buy a PRO-800; do not start before I say so)
Hardware facts **[VERIFIED from the official README]**:
- MCU **STM32F446** (schematic: 32F446VCT6, LQFP-100), max **180 MHz**, **128 kB RAM**, external **8 MHz** crystal (Y1).
- Bootloader fixed at **0x08000000** and write-protected. **The app's vector table / reset handler must be linked at `0x08010000`.** Max app size **128 kB**. Format: raw `.bin`.
- Pack: `PRO800_update_gen <fw.bin> <fw.syx> <XYZ>` (e.g. `001`); output must end in `.syx`. The packer encrypts; never try to bypass or re-implement it. Output is deterministic: one header SysEx `F0 00 20 32 00 01 24 00 0A …` then 255-byte data messages `F0 00 20 32 00 01 24 00 0B 00 <block> …`.
- Update mode: hold **panel buttons 0 and 7 while powering on**, or send `F0 00 20 32 00 01 24 00 03 30 F7`. Send the .syx with any SysEx tool (Linux `amidi`, macOS SysEx Librarian, Windows MIDI-OX); keep the rate ≤ **9.64 kB/s**. Failed/corrupt image ⇒ unit stays in update mode; recover with Behringer SynthTribe.
- **ABSOLUTE RULE: never connect a JTAG/SWD probe to unlock or change flash protection — it mass-erases the bootloader and bricks the unit. Never write below 0x08010000. Never touch option bytes.**

Linker script (verified to build and pack on Linux with Arm GNU Toolchain 14.2.rel1):
```ld
MEMORY {
  FLASH (rx)  : ORIGIN = 0x08010000, LENGTH = 128K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
ENTRY(Reset_Handler)
SECTIONS {
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > FLASH
  .data : AT(_etext) { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM
  .bss  : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  _estack = ORIGIN(RAM) + LENGTH(RAM);
}
```
Startup must: copy .data, zero .bss, set `SCB->VTOR = 0x08010000`, then fully re-initialise clocks (HSE 8 MHz → PLL), disable/clear all NVIC IRQs and peripherals the bootloader might have left on **[bootloader hand-off state UNVERIFIED]**. Build flags: `-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Os -ffunction-sections -fdata-sections -Wl,--gc-sections` (enable the FPU in CPACR before any float code). Use CMSIS device headers (Apache-2.0) + LL or registers; **TinyUSB** (MIT) for USB-MIDI.

H0 steps: (1) document a SysEx backup of all patches and the stock FW version (version request `08 00` → reply `09 00 xx yy zz`); (2) rehearse recovery (buttons 0+7 → SynthTribe → stock FW) *before* any custom flash; (3) flash a stub that sends a MIDI note + SysEx ping on DIN MIDI out (UART4_TX on PA0 @31250 baud **[UNVERIFIED AF]**) and lights all HT16K33 segments (I2C1 PB6/PB7, scan 0x70–0x77 **[address UNVERIFIED]**); (4) recover to stock.
- **Accept when:** a CI job proves `.isr_vector` is at 0x08010000, image ≤ 131072 bytes, packer produces a valid-looking .syx; on hardware one full flash → observe → recover loop is logged in `docs/hardware-log.md`.

### H1 — PRO-800 HAL (reproduce stock-like behaviour)
Main-PCB pin map from the official schematic. Net names are **[VERIFIED as printed]**; peripheral/alternate-function choices are **[UNVERIFIED — check STM32F446 datasheet DS10693 / RM0390]**:
- MIDI: PA0 → AND gate U4A → MIDI OUT/THRU (UART4_TX); MIDI IN via TLP2368 opto → 74LVC1G125 (OE = **PB4 MIDI_SEL**) → probably PA1 (UART4_RX).
- USB OTG_FS: PA11/PA12, VBUS sense PA9.
- 16-bit CV DAC **DAC80501** (U46): DAC_MOSI on **PB0**, plus two lines that look like **PC10/PA15** (SCLK/SYNC) — likely SPI3. Output 16BIT_DAC1 feeds four **74HC4051** demuxes + TL064 S&H buffers → per-voice `OSC_1A..8A, OSC_1B..8B, FIL_1..8, AMP_1..8`. 4051 address **SH0/SH1/SH2 = PB5/PB9/PB10**, enables **EN4–EN8 = PB12–PB15, PD6** (through 74HCT244), **EN9 = PD7**. Two more 4051s give globals `PW_A_CV, PW_B_CV, VOL_A_CV, VOL_B_CV, RES_CV, NOISE_CV, M_VOL_CV, PMOD_OSC_B_CV, EXT_FIL_CV`. Internal 12-bit DACs PA4/PA5 → 12BIT_DAC1/2 (used on sheet 3). S&H outputs must be refreshed continuously from a timer ISR (like P600fw `sh.c`).
- Pots: three 74HC4051 with address **PM0–PM2 = PD0–PD2**, enables **EN1–EN3 = PD3–PD5**, outputs **VMUX1–3 → PC0–PC2** (ADC); CUTOFF pot → **PC3**, NOISE pot → **PA2** (direct ADC). 26 pots total.
- VALUE encoder → 74HC14 → **PE9 / PE11** (TIM1 encoder mode).
- **HT16K33** (U35) on I2C1 **PB6/PB7** via BSS138 level shifters to 5 V: drives the 7-segment digits, the button LEDs (map: K5/K6/K7 × COM4–7 = PRESET/SYNC SOURCE/SEQ2, PERF/SYNC CLOCK/ARP UP-ON, SETTINGS/REC/ARP-ASSIGN, TUNE/SEQ1/POWER) and scans the membrane buttons (K1–K13 × COM0–7).
- Latching panel switches to GND (enable pull-ups): OSC1_SAW PE0, SAW_A PE7, TRI_A PE8, SYNC PE10, SAW_B PE12, TRI_B PE13, PMOD_FA PE14, PMOD_FIL PE15, OSC1_TRI PA8, OSC1_SQ PC4, OSC2_SAW PC5, OSC2_TRI PC6, OSC2_SQ PC7, PW_A_B PC8, F_A_B PC9, FREQ_A PC11, LFO_SHAP PC12, LFO_FILTER PC13, FILTER PC14, UNISON PC15, keytrack HALF PB1 / FULL PB2 (3-way). Some of these nets also go to the voice board as control lines — determine direction per pin before driving anything as output **[UNVERIFIED]**.
- Auto-tune: **TUNE_SIG PA6**, comparator **TUNE_CPR** on PB8 (via R32) — timer input capture, port P600fw `tuner.c` logic.
- Storage: **25M01** 128 kB SPI EEPROM: SCK PE2, MISO PE5, MOSI PE6, CS PE4, WP PE1, HOLD PE3 (SPI4). **Read-only until the stock layout is understood; then use a separate region; never wipe user patches without an explicit backup.**
- SYNC IN jack → 74HC14 → PD14/PD15; SYNC_SEL PD8; MIDI channel strap MIDI_CH_0..3 = PD9–PD12; BACKPANEL_PRESENCE PD13; pedal detect PA7.
- No keybed exists (it's a desktop/Eurorack module): notes come only from DIN/USB MIDI.
- Unknown: voice-board schematic, CV scaling (rely on auto-tune), HT16K33 segment map, bootloader hand-off, how stock FW jumps to update mode (until known, recovery = buttons 0+7).
Bring-up order, each with a SysEx debug dump command: pots/switches/encoder/buttons → display text; DAC + one S&H channel (measure with audio interface); one voice gated by MIDI; auto-tune; 8 voices; USB-MIDI; EEPROM read.
- **Accept when:** plays 8 voices in tune (±3 cents after auto-tune) from DIN and USB MIDI, all panel controls work, patches save/load in our region, and the same golden CvFrame tests pass on host and target.

### H2 — ship V2 features on hardware
- **Accept when:** a tagged release publishes the `.syx`, the `.bin`, full source (GPL obligations), install + recovery instructions, and a demo video script, all under ElfChords / Garth Elfstrom.

## 4. How to work
- Start with **V0 only**. Before writing code, reply with: (a) your licence/framework choice and why, (b) the repo tree, (c) the first 5 commits you'll make.
- One concern per commit; run tests after each; never claim something works without showing the command output.
- Keep a `docs/DECISIONS.md` (ADR-style) and `docs/UNVERIFIED.md` listing every hardware assumption with where it came from.
- Ask me before: buying anything, choosing a final product name, publishing anything, or any step that touches real hardware.
