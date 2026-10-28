# ElfChords P8 — Roadmap

Phases are gated on verification, not dates. A phase is done when STATUS.md
can state its claim with evidence behind it. Full source roadmap (2026-10-28)
is preserved in `docs/input/ROADMAP-source-2026-10-28.md`.

## Phase V0 — Core on the laptop
- CMake scaffold; import P600fw logic into `core/p600fw_derived/` with shims; `hal_mock` recording CvFrames.
- Unit tests: envelopes, LFO, assigner (8 voices, unison), glide, arp, sequencer, MIDI clock 24 PPQN, MIDI parser (running status, SysEx).
- `tools/render`: MIDI file → CvFrame log → placeholder DSP → WAV.
- Gate: `cmake --build && ctest` green; ARM cross-compile of `core/` (`arm-none-eabi-gcc`, Cortex-M4 hard-float) green; render produces an audible 8-voice chord WAV; a golden-CvFrame snapshot test exists.

## Phase V1 — Playable plugin
- JUCE (AGPL-3.0) plugin: VST3 + AU + CLAP + standalone; panel UI (26 pots, switches, VALUE encoder, display); 400-patch bank; DAW state; PRO-800-compatible MIDI CC map; PRO-800 SysEx patch import/export.
- DSP voice model with per-voice variance, poly-mod, sync, unison, glide.
- Gate: `pluginval` strictness 10 + Steinberg `validator` pass; loads/plays in Reaper (and Live/Logic where available); SysEx patch round-trips; CPU < 5% of one core at 48 kHz/128 with 8 voices.

## Phase V2 — ElfChords features (in core, so they later ship on hardware)
In priority order, each behind a setting, each with a unit test + demo preset:
1. LFO tempo-sync to MIDI clock. 2. Mod wheel → VCF. 3. Sequencer SysEx dump/load + more stored sequences. 4. Latched seq/arp transpose. 5. Small mod matrix. 6. Externally triggered randomize. 7. Two-unit 16-voice chaining. 8. Avoid the known stock-firmware bugs (Defros list, fw 1.4.6).
- Gate: each feature has a unit test, a demo preset and a CHANGELOG line.

## Phase H0 — Hardware hello (only after Garth buys a unit and says go)
- SysEx backup of all 400 patches + stock FW version; rehearse recovery (buttons 0+7 → SynthTribe → stock) *before* any custom flash.
- Flash a stub (MIDI note + SysEx ping on DIN out, all HT16K33 segments lit); confirm pin assumptions; recover to stock.
- Gate: one full flash → observe → recover loop logged in `docs/hardware-log.md`; CI proves `.isr_vector` at `0x08010000`, image ≤ 131072 B, packer emits valid header+data messages.

## Phase H1 — PRO-800 HAL (reproduce stock-like behaviour)
- Drivers in bring-up order, each with a SysEx debug readout: pots/switches/encoder/buttons → display; DAC80501 + S&H refresh; one voice; auto-tune; 8 voices; USB-MIDI; EEPROM read-only first, then own storage region.
- Gate: 8 voices in tune (±3 cents after auto-tune) from DIN + USB MIDI, all panel controls work, patches save/load in our region, golden CvFrame tests pass on host and target.

## Phase H2 — Ship V2 features on hardware
- Gate: tagged release with `.syx` + `.bin` + full source (GPL), install + recovery instructions, demo — all under ElfChords / Garth Elfstrom.

## Explicitly later (not this roadmap)
- Closed-source commercial plugin — would require a clean-room core replacing `core/p600fw_derived/`; not planned.
- Eurorack-specific variant behaviour — the unit detects panel presence (`BACKPANEL_PRESENCE`); revisit only if a rack unit is bought.
- Anything requiring SWD/JTAG on the PRO-800 — permanently out, see PROJECT_TRUTH non-negotiable 2.
