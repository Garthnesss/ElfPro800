# ElfChords × PRO-800-class poly — Roadmap (VST-first, firmware-second)

Author/brand for public releases: **ElfChords — Garth Elfstrom**.
Prepared 2026-10-28 (America/Los_Angeles). Status tags: **[VERIFIED]** = read from a primary source or reproduced on the box; **[INFERRED]** = read off the schematic / reasoned, needs hardware confirmation; **[UNKNOWN]** = not found anywhere.

JARTH does **not** own a PRO-800 yet. So the plan is: build a **shared portable core** now, ship it first as a **VST3/AU/CLAP plugin** (no hardware needed), then reuse the same core as **PRO-800 replacement firmware** once a unit is bought.

---

## 1. What was released (MusicTribeOpenSource/PRO800-Firmware-Packer)

Repo: https://github.com/MusicTribeOpenSource/PRO800-Firmware-Packer (cloned to `./PRO800-Firmware-Packer`, HEAD `c845054`, 2026-10-23). Contents in full:

| File | What it is |
|---|---|
| `README.md` | The only technical doc (summarised below) |
| `linux/PRO800_update_gen` (x86-64 ELF), `macos arm64/PRO800_update_gen`, `windows/PRO800_update_gen.exe` | **Closed, binary-only** packer. No source. |
| `Schematics/PRO800_Main_Schematics.pdf` | 5 sheets of the **main PCB** only: 1 MCU, 2 Sample&Hold 1, 3 Sample&Hold 2 + voice-board connectors + tuner, 4 Surface (pots/LEDs/display/buttons), 5 Power/USB/rear. **Voice-board schematic is NOT included.** |
| `LICENSE` | AGPL-3.0 text |
| `DISCLAIMER.txt` | Music Tribe copyright/no-warranty; says bootloader is write-protected and an incorrect update "can be recovered by loading a valid firmware image" |

There is **no stock PRO-800 firmware source, no SDK, no HAL, no linker script, no example firmware, no memory map** beyond the README numbers. [VERIFIED — full file list above]

Licensing oddity: the repo carries an AGPL-3.0 LICENSE but only ships binaries of the tool. The AGPL would apply to the packer itself; a firmware you write and merely *process* with it is your own work. (Not legal advice.) [VERIFIED file contents; interpretation is mine]

### README facts [VERIFIED, quoted values]
- MCU: **STMicroelectronics STM32F446**; schematic part marking **32F446VCT6** (LQFP-100). Max **180 MHz**, **128 kB RAM**.
- **Reset handler / app image must be linked at `0x08010000`.** Bootloader is fixed at **`0x08000000`** (so 0x08000000–0x0800FFFF = sectors 0–3 belong to the bootloader [INFERRED from F446 sector layout]).
- App format: raw **.bin**, **max 128 kB**.
- Packer: `PRO800_update_gen <fw.bin> <out.syx> <XYZ version>` (e.g. `101`). Version is for the bootloader only.
- Enter update mode: **hold panel buttons 0 and 7 at power-up**, or send SysEx **`F0 00 20 32 00 01 24 00 03 30 F7`** (unit restarts into update mode).
- Send the .syx with any SysEx tool (MIDI-OX, Bome SendSX, SysEx Librarian). Max rate **9.64 kB/s**.
- Bootloader area is write-protected; failed/corrupt update ⇒ unit **stays in update mode**; recover via **SynthTribe** (Behringer's updater app) by loading stock firmware.
- **Never unlock flash read-protection via JTAG/SWD — that mass-erases everything including the bootloader = brick.**

### Packer behaviour (reproduced on the box) [VERIFIED]
- Linux binary runs on x86-64 glibc (needs only libc). Strings show `P0E9I` (the PCB/project code also in the schematic title block) and `BEHRINGER PRO 800`.
- Output is **deterministic** (same input ⇒ byte-identical .syx).
- For a 1,136-byte test image it emitted **7 SysEx messages (1,473 bytes)**: one **header** message `F0 00 20 32 00 01 24 00 0A …` (66 bytes, contains `P0E9I`, `BEHRINGER PRO 800`, version) followed by **data** messages `F0 00 20 32 00 01 24 00 0B 00 <block#> …` of 255 bytes each (last one shorter). Payload is encrypted/obfuscated — do **not** try to re-implement; always use the official tool.
- `00 20 32` = Behringer's SysEx manufacturer ID; `00 01 24 00` matches the device header the community found for normal PRO-800 SysEx (samstaton/pro800).

## 2. Hardware map from the schematic (main PCB)

See `notes/schem_*.png` crops. Pin → net names are **[VERIFIED as printed on sheet 1]**; the *function/peripheral* column is **[INFERRED]** (STM32F446 alternate-function assumptions must be checked against the RM0390/DS10693 datasheet before use).

| Pin(s) | Net | Function (inferred) |
|---|---|---|
| PH0/PH1 | Y1 **8 MHz** crystal | HSE 8 MHz → PLL to 180 MHz (or 168 MHz for exact 48 MHz USB clock) |
| PA11/PA12, PA9 | USB_DM/DP_IN, USB_DEVICE_VBUS_IN | USB OTG_FS device (USB-MIDI) |
| PA0 | → U4A 74LVC1G08 input B; output **MIDI_THRU** | MIDI OUT data = **UART4_TX** (AND-ed with soft-thru) |
| PA1 (likely) | from U5A 74LVC1G125 (opto TLP2368 → buffer) | MIDI IN = **UART4_RX** [INFERRED] |
| PB4 | MIDI_SEL | OE of U5A (gates MIDI-in into the thru path) |
| PB0, PC10, PA15 | DAC_MOSI + 2 unlabeled lines to U46 | **DAC80501** 16-bit DAC (SYNC/SCLK/SDIN) — likely SPI3 MOSI/SCK/NSS [INFERRED] |
| PA4, PA5 | → U16A/B (4580 op-amp) → **12BIT_DAC1/2** | internal STM32 12-bit DACs (sheet 3 uses 12BIT_DAC1) |
| PB5, PB9, PB10 | → U28 74HCT244 → **SH0, SH1, SH2** | 3-bit address for all **74HC4051** CV demuxes (5 V) |
| PB12–PB15, PD6 | → U28 → **EN4–EN8** | 4051 enables for the S&H banks (OSC_A×8, OSC_B×8, FIL×8, AMP×8, globals) |
| PD3–PD5, PD7 | **EN1–EN3, EN9** | enables for pot muxes (EN1–3) / global S&H (EN9_5V) |
| PD0–PD2 | **PM0–PM2** | address for 3× 74HC4051 **pot muxes** |
| PC0–PC2 | **VMUX1–3** | ADC inputs from the pot muxes (8 pots each = 24 pots) |
| PC3, PA2 | **CUTOFF_ADC, NOISE_ADC** | two pots wired straight to ADC |
| PE9, PE11 | **VALUE_A_E, VALUE_B_E** | VALUE rotary encoder via 74HC14 — TIM1_CH1/CH2 encoder mode [INFERRED] |
| PB6/PB7 | **I2C1_SCL_33/SDA_33** → BSS138 shifters → 5 V I2C | **HT16K33** (U35, "16K33"): 7-seg digits (DS1, DS2…), panel LEDs and the **membrane button matrix** (K1–K13 × COM0–7). I2C address **[UNKNOWN]** (A0–A2 share ROW pins; scan 0x70–0x77) |
| PE2, PE5, PE6, PE4, PE1, PE3 | **EEP_SCK, EEP_MISO, EEP_MOSI, EEP_CS, EEP_WP, EEP_HOLD** | **25M01** 1-Mbit (128 kB) SPI EEPROM (U11) = patch/sequence/settings storage — SPI4 [INFERRED] |
| PA8, PC4–PC9, PC11–PC15, PE0, PE7, PE8, PE10, PE12–PE15, PB1, PB2 | OSC1_TRI, OSC1_SQ, OSC2_SAW, OSC2_TRI, OSC2_SQ, PW_A_B, F_A_B, FREQ_A, LFO_SHAP, LFO_FILTER, FILTER, UNISON, OSC1_SAW, SAW_A, TRI_A, SYNC, SAW_B, TRI_B, PMOD_FA, PMOD_FIL, HALF, FULL | **Latching panel switches to GND** (S8 = 3-way keytrack Off/Half/Full, S13–S16 etc.) → GPIO inputs with pull-ups; some nets (SAW_A/TRI_A/SAW_B/TRI_B/SYNC/PMOD_*) also go to the voice board as **digital control lines** — direction per pin must be confirmed [INFERRED] |
| PA6, PB3/PB8 | **TUNE_SIG**, **TUNE_CPR** (via R32 0 Ω; R40 removed) | auto-tune comparator input → timer input-capture (like P600 tuner) |
| PD14, PD15 | via 74HC14 from **SYNC IN** jack tip/ring | external clock/sync input |
| PA7, PA3? | BACKPANEL_PEDAL_DT / sustain pedal | footswitch detect / sustain [INFERRED for PA3] |
| PD9–PD12 | **MIDI_CH_0..3** | 4-bit MIDI-channel strap/switch (sheet 5) |
| PD13 | BACKPANEL_PRESENCE | detects desktop rear panel vs Eurorack use |
| PD8 | SYNC_SEL | sync source select |
| PA13/PA14 | SWDIO/SWCLK | SWD header — **do not use to unlock** |

Sheet 2: four 74HC4051 → 32 TL064-buffered S&H outputs **OSC_1A…OSC_8A, OSC_1B…OSC_8B, FIL_1…FIL_8, AMP_1…AMP_8**, fed from **16BIT_DAC1** (DAC80501). Plus audio out/phones mute (MUTE, PH_MUTE).
Sheet 3: two more 4051s → globals **PW_A_CV, PW_B_CV, VOL_A_CV, VOL_B_CV, RES_CV, NOISE_CV, M_VOL_CV, PMOD_OSC_B_CV, EXT_FIL_CV**; FILTER CV IN jack; tuner; connectors X12/X13 to the voice board.
Sheet 4: pots: AMP ATT/DEC/SUS/REL, FIL ATT/DEC/SUS/REL, FIL ENV AMT, M.TUNE, LFO INIT AMOUNT, LFO FREQ, GLIDE, MVOL, B LEVEL, OSC B FREQ, FINE, A LEVEL, FREQ A, RESONANCE, A PW, B PW, FREQ B, …, plus NOISE and CUTOFF direct. LED map (HT1_K5–K7 × COM4–7): PRESET, SYNC SOURCE, SEQ2 / PERF, SYNC CLOCK, ARP UP-ON / SETTINGS, REC, ARP-ASSIGN / TUNE, SEQ1, POWER.

**Important correction to the brief:** the PRO-800 is a **desktop/Eurorack module with no keybed** — there is no key scanning; notes come only from MIDI DIN / USB-MIDI. [VERIFIED: Thomann spec list "MIDI in/out (5-pole DIN) and USB-MIDI", 80 HP Eurorack; no keyboard on sheet 4]

So a from-scratch firmware must drive: pot mux + ADC scanning (26 pots), latching switch GPIOs, VALUE encoder, HT16K33 (display/LEDs/buttons over I2C), DAC80501 + 4051 S&H refresh for 32 per-voice + ~9 global CVs, voice-board digital lines (waveform/sync/polymod/unison), auto-tune via TUNE_SIG/TUNE_CPR, MIDI UART + USB-MIDI device, 25M01 EEPROM storage, sync in, and **software envelopes/LFOs/glide/arp/seq** (as in P600fw everything modulation-related is computed by the MCU and written as CVs).

## 3. The big shortcut: GliGli's P600fw

- **Lineage [VERIFIED]:** Behringer & GliGli announced a collaboration on 2019-09-06 (Synth Anatomy, Matrixsynth). Behringer, 2020-09-17: "Together with GliGli we have taken this beautiful synth to the next level. The PRO-800 is now a fully polyphonic, 8-Voice Dual VCO synth" (Matrixsynth). Launch text 2022-12-07: "PRO-800 is also fitted with the iconic GliGli modification as standard" (Matrixsynth). Community SysEx analysis finds PRO-800 patch fields "almost in the order used by GliGli" (samstaton/pro800).
- **But the PRO-800's own firmware source is not public.** No `pro800` firmware repo exists on GitHub (searched 2026-10-28; only editors/SysEx docs: samstaton/pro800, steinachim/pro800_manager_plugin, lsim/pro800cc). It is **not** proven that Behringer's code is a port of the public repo (it may be a GliGli-authored proprietary rewrite).
- **Source:** https://github.com/gligli/p600fw (cloned to `./p600fw`, last push 2026-04-13; ~7.6 kLOC in `common/`; maintained 2022+ by Imogen / image-et-son). Contributors: GliGli 319 commits, image-et-son 182, Ricard Wanderlof 48, Seth Nickell 25, others.
- **License [VERIFIED]:** source **GPL-3.0** ("Everything is under GPL v3 license, except files that have their own license in the header"); binaries "free for non commercial use" (gligli.github.io/p600fw). `xnormidi/` is GPL-3.0 too.
- **Portability test on the box [VERIFIED]:** with a 10-line shim (`PROGMEM`, `pgm_read_byte`, `CYCLE_WAIT`, `BLOCK_INT`, `MDELAY`, `print.h` stubs) the logic files `adsr, lfo, assigner, arp, seq, utils, clock, midi, storage, synth, ui` **all compile for arm-none-eabi (Cortex-M4)**, and all but `synth.c` (missing non-standard `itoa`) compile for host x86-64 gcc. Hardware-coupled files are only `dac.c, display.c, potmux.c, scanner.c, sh.c, tuner.c, uart_6850.c` (+ 3 calls in `synth.c`), which talk to the original Prophet-600 Z80 I/O map via `mem_write/io_write`. `common/hardware.h` literally says "Low level interface, implemented by firmware or **mockup**".
- **Verdict:** P600fw is a **usable base for the synth logic** (voice assigner, envelopes, LFO, arp, seq, MIDI, patch model, UI state machine) on both targets. The hardware layer must be rewritten for the PRO-800 (direct STM32 GPIO, DAC80501 + 4051, HT16K33, 26 pots, encoder, 25M01). It is **not** a drop-in PRO-800 firmware.
- **License consequence:** anything that includes P600fw code must be **GPL-3.0** (or AGPL-3.0, which is combinable with GPL-3.0 per §13 of both licences). That's fine for open firmware and an open-source plugin (you may still sell GPL binaries, but must offer source). A **closed-source commercial plugin cannot contain P600fw code** — that would need a clean-room core.

## 4. Architecture (dual target)

```
elfchords-p8/                      (working title — final name TBD, no Behringer/Prophet marks)
├─ core/        portable C99, no malloc, no OS, fixed control rate (e.g. 2 kHz)
│   ├─ synth logic (from P600fw or clean-room): assigner, adsr, lfo, glide, arp, seq, patch model, ui state machine, midi parser
│   └─ hal.h   abstract interface:
│        inputs : pots[26] (0..65535), switches bitfield, encoder delta, buttons (HT16K33 matrix), midi bytes, sync ticks, tune-capture
│        outputs: CvFrame{ oscA[8], oscB[8], fil[8], amp[8], pwA, pwB, volA, volB, res, noise, mvol, pmodB, extFil } (uint16),
│                 voice gate/waveform/sync bits, display chars, LED bits, midi out bytes, storage page r/w
├─ targets/plugin/   JUCE (or DPF/iPlug2) VST3/AU/CLAP + standalone
│   ├─ HAL impl: GUI knobs/switches → pots/switches; DAW MIDI → midi bytes; CvFrame → DSP
│   └─ dsp/: analog-voice model per voice: 2 VCOs (saw/tri/pulse, hard sync, poly-mod FM), noise, 24 dB/oct LPF w/ resonance, VCA,
│           per-voice tolerance/drift ("8 slightly different voices"), CV slew to mimic S&H refresh
├─ targets/pro800/   bare-metal STM32F446 (CMSIS + LL or register level; TinyUSB for USB-MIDI)
│   ├─ link.ld (FLASH ORIGIN 0x08010000 LENGTH 128K; RAM 0x20000000 128K), startup sets SCB->VTOR = 0x08010000
│   └─ HAL impl: ADC+4051 scan, GPIO, TIM1 encoder, I2C1 HT16K33, SPI DAC80501 + 4051 S&H refresh ISR, UART4 MIDI, USB-MIDI, SPI4 25M01, tuner capture
└─ tests/  host unit tests + golden CvFrame snapshots shared by both targets
```

Key idea: the core outputs a **virtual CV frame**. The firmware writes it to the DAC/S&H; the plugin feeds it to the DSP voice model. Same core, same patches, same behaviour, testable on a laptop today.

Bonus: import real PRO-800 patches into the plugin via the community-documented SysEx format (samstaton/pro800 `pro800syx.md`; 8-bit-in-7-bit packing, patch dump `77 xx yy` / response `78 xx yy …`, fields from offset 5). [format VERIFIED as documented by community, incomplete]

## 5. Licensing & naming

| Component | License | Implication |
|---|---|---|
| P600fw (if reused) | GPL-3.0 | Whole plugin & firmware must be GPL-3.0 (or AGPL-3.0); publish source; keep copyright headers; credit GliGli, Imogen et al. |
| JUCE 8 | AGPL-3.0 **or** commercial (Starter free ≤ US$20k/yr revenue from the JUCE product, Indie $40/mo or $800 perpetual ≤ $300k, Pro) | GPL core ⇒ use JUCE under AGPL-3.0 (open source) — compatible. Closed-source ⇒ needs JUCE licence **and** a clean-room core. |
| VST3 SDK 3.8+ | **MIT** (since 2025-10-20) | Fine for both. If you use the "VST" name/logo follow Steinberg usage guidelines ("VST is a registered trademark of Steinberg Media Technologies GmbH"). |
| Alternatives | DPF (ISC), iPlug2 (zlib-style), CLAP (MIT) | Permissive; avoid AGPL's network clause if preferred. |
| TinyUSB | MIT | Fine for firmware. |
| STM32 CMSIS / LL | Apache-2.0 / BSD-3 | Fine. |
| PRO800_update_gen | binary; repo says AGPL-3.0 | Use as a tool only; don't redistribute modified copies; your firmware stays yours. |
| Behringer EULA (SynthTribe etc.) | proprietary | Applies to their software, not to your own firmware; flashing custom FW is officially sanctioned by this release but at your own risk (DISCLAIMER). |

Trademarks: do **not** use "Behringer", "PRO-800", "Prophet", "Sequential", "SynthTribe", "Music Tribe", "GliGli" in product names, logos or plugin IDs. Nominative wording in descriptions is the usual practice ("custom firmware for the Behringer PRO-800", "inspired by early-80s 8-voice polysynths"). Plugin manufacturer = **ElfChords**, author **Garth Elfstrom**. (Not legal advice — check before selling.)

## 6. What to buy (when ready)

- **Behringer PRO-800**: new ~US$235 (Thomann US listing) to US$399 (Sweetwater); used ~US$290 / €260 median, most €250–€300 (Doirado, 37 sales, 90 days to 2026-10-25). It ships with a 12 V DC 1.2 A PSU (Thomann).
- **USB cable** for the PRO-800's USB-MIDI port (port type: check the unit; not confirmed here) — SynthTribe recovery and SysEx flashing run over USB.
- **A MIDI controller/keyboard** (the unit has no keys).
- Recommended: a **class-compliant USB-MIDI interface** with DIN in/out (test DIN MIDI, soft-thru, and a second flashing path), and an **audio interface** to record outputs for tuning/regression tests.
- Software: SynthTribe (stock firmware recovery), a SysEx sender (Linux `amidi`, macOS SysEx Librarian, Windows MIDI-OX).
- **Do not buy/attach an SWD probe to "unlock" anything.** Debug via MIDI/SysEx logging and the display.

## 7. Risks

| Risk | Mitigation |
|---|---|
| Bricking via SWD/RDP unlock (mass erase incl. bootloader) | Never connect a debugger to change protection. Flash only through bootloader SysEx. |
| Bad custom FW boots but hangs/doesn't answer SysEx | Bootloader is entered at power-up by holding **buttons 0 + 7** independently of the app [VERIFIED README] — rehearse this recovery **before** the first custom flash. Implement the `… 03 30 F7` reboot-to-update command in our FW only once the mechanism is known [UNKNOWN how stock FW hands off to the bootloader]. |
| Wrong CV levels damaging nothing but sounding wrong; voice-board details unknown | Voice-board schematic not released → calibrate empirically with auto-tune; start with stock-like value ranges observed via MIDI CC/SysEx. |
| Bootloader leaves clocks/IRQs/USB configured | App startup must fully re-init RCC, disable all IRQs, set VTOR, re-enumerate USB. [UNKNOWN bootloader state] |
| Losing user patches in EEPROM | Dump all 400 patches with SysEx before flashing; our FW must not write the 25M01 until its storage format is proven (read-only first). |
| GPL contamination of a commercial plugin | Decide licence model **before** writing code (§5). |
| Trademark complaints | ElfChords naming only. |
| 128 kB flash limit | P600fw core is AVR-sized (fits easily); watch float/libm/USB stack size; CI size gate. |

## 8. Phased plan (VST-first)

**Phase V0 — core on the laptop (start today)**
- Repo scaffold, CMake, core built from P600fw logic files with HAL shim; host unit tests (envelopes, LFO, assigner, arp, seq vs MIDI clock).
- CLI renderer: MIDI file → CvFrame log → simple DSP → WAV.
- Exit: `ctest` green; render of a test MIDI file produces 8-voice audio; ARM cross-compile of `core/` succeeds.

**Phase V1 — playable plugin**
- JUCE (AGPL) or DPF plugin: VST3 + AU (macOS) + CLAP + standalone; panel UI mirroring the hardware controls (26 pots, switches, VALUE encoder, number buttons, 4-char display); 400-patch bank; host automation; MIDI CC map matching the PRO-800 (lsim/pro800cc, samstaton docs).
- DSP voice model with per-voice variance, poly-mod, sync, unison, glide.
- Import/export PRO-800 SysEx patches.
- Exit: passes `pluginval` (strictness 10) and Steinberg `validator`; loads in Reaper/Ableton/Logic; patch import round-trips.

**Phase V2 — differentiating features (in core, so they later ship on hardware too)** — top owner requests (sources):
1. **Tempo-synced LFO / LFO sync to MIDI clock** — Behringer Ideas board "Tempo-Sync'd LFOs on Pro-800"; Gearspace p.190 "Sync LFO to MIDI".
2. **Mod wheel → VCF routing** — Behringer Ideas board "PRO 800 - Route ModWheel to VCF".
3. **Sequencer SysEx import/export and more stored sequences** (only SEQ1/SEQ2 today) — Defros wishlist #1 (samstaton/pro800).
4. **Latched seq/arp transpose mode** — Defros wishlist #3.
5. **Deeper mod matrix (e.g. osc/env/aftertouch as sources to more destinations)** — Gearspace p.115.
6. **Randomize triggered externally / during sequences** — Gearspace p.115.
7. **Two-unit chaining (16 voices)** — Gearspace p.115.
8. **Fix stock bugs**: Arp-Mode CC#49 not sent; Subdivision CC#50 collides with OSC A Freq LSB; per-voice manual retune not saved; patch name not copied on save-as; pedal polarity not in system dump; seq-speed record procedure broken — Defros bug list (fw 1.4.6).
9. Better use of the small LED display / clearer menus — Gearspace p.143.

**Phase H0 — hardware hello (after buying)**
- Back up all patches/sequences via SysEx; note stock FW version (SysEx `08 00` → `09 00 xx yy zz`).
- Rehearse recovery: buttons 0+7 at power-up → SynthTribe reinstall stock.
- Flash `phase0_stub` (already builds & packs on the box): sends MIDI note + SysEx ping on DIN out, lights all HT16K33 segments. Confirm pin assumptions (UART4 on PA0, I2C1 on PB6/7, HT16K33 address).
- Recover to stock again. Exit: one full flash→observe→recover loop documented.

**Phase H1 — reproduce stock behaviour on hardware** — HAL drivers one by one, each with a SysEx debug readout: pots/switches/encoder/buttons → display; DAC80501 + S&H refresh; one voice gated by MIDI; auto-tune; 8 voices; USB-MIDI; EEPROM read of stock patches (read-only), then own storage region.
**Phase H2 — ship V2 features on hardware**; public beta `.syx` under ElfChords with source (GPL).

## 9. Test plan

Without hardware: host unit tests on core; golden CvFrame snapshot tests (same MIDI in ⇒ same CV out on both targets); fuzzing the MIDI/SysEx parser; plugin `pluginval` + Steinberg validator in CI; offline WAV renders with spectral/level asserts; firmware CI: cross-compile, check `.isr_vector` at 0x08010000, `size` ≤ 128 kB, run `PRO800_update_gen` and assert it produces a header (`…24 00 0A`) + data (`…24 00 0B`) messages; optional Renode STM32F4 emulation with mocked I2C/SPI peripherals.
With hardware: recovery drill; per-driver bring-up checks; MIDI loopback (DIN out → interface → DIN in); scope/audio-interface tuning accuracy (cents per voice per octave); pot/encoder sweep logs; 8-hour soak with random MIDI; power-cycle storage integrity; compare against stock recordings.

## 10. Known vs unknown

Known: MCU, app address, size limits, update procedure, recovery path, main-PCB schematic, packer usage, PRO-800 SysEx header and community patch format, P600fw logic portable.
Unknown: voice-board schematic and CV scaling; exact peripheral/AF assignment of several pins; HT16K33 I2C address and segment map; bootloader hand-off state and how stock FW triggers update mode; whether bootloader verifies a signature beyond the packer's encryption; stock EEPROM layout; USB connector type; whether stock FW is derived from the public P600fw code.

## Sources
- MusicTribeOpenSource/PRO800-Firmware-Packer README/DISCLAIMER/schematic: https://github.com/MusicTribeOpenSource/PRO800-Firmware-Packer
- GliGli P600fw: https://github.com/gligli/p600fw , https://gligli.github.io/p600fw/
- Behringer × GliGli (2019-09-06): https://synthanatomy.com/2019/09/behringer-gligli-announced-collaboration-to-develop-analog-synths.html , https://www.matrixsynth.com/2019/09/gligli-and-behringer-announce.html
- "Together with GliGli…" (2020-09-17): https://www.matrixsynth.com/2020/09/new-behringer-pro-800-in-works.html
- "GliGli modification as standard" (2022-12-07): https://www.matrixsynth.com/2022/12/behringer-introduces-pro-800-destop.html
- Community SysEx / bugs / wishlist: https://github.com/samstaton/pro800 (pro800syx.md, defros/firmware-1.4.6-defros.md — copy in notes/)
- Behringer Ideas: https://ideas.behringer.com/p/tempo-sync-d-lfos-on-pro-800 , https://ideas.behringer.com/p/pro-800-route-modwheel-to-vcf
- Gearspace PRO-800 thread pp.115, 143, 190: https://gearspace.com/threads/behringer-pro-800.1301127/page-115 (…/page-143, …-a-190.html)
- Specs/prices: https://www.thomannmusic.com/behringer_pro_800.htm , https://www.sweetwater.com/store/detail/BehrPro800--behringer-pro-800-8-voice-polyphonic-analog-synthesizer , https://doirado.com/behringer/pro-800-desktop-8-voice-synthesizer
- VST3 MIT: https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License.html
- JUCE 8 licence: https://juce.com/legal/juce-8-licence/ , https://juce.com/get-juce/
