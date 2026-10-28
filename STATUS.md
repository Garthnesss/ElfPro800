# ElfChords P8 — Status

**Last updated:** 2026-10-28
**Phase:** Pre-V0 — pre-code proposal delivered, awaiting Garth's go on licence/framework + first commits.

## Where things stand
- Source roadmap + coding prompt — **RECEIVED** (2026-10-28, Garth): preserved in `docs/input/`; this project's canonical docs are derived from them.
- Upstream claims — **VERIFIED on this VM (2026-10-28)**: packer repo cloned at HEAD `c845054` (2026-10-23); tree matches the roadmap exactly (README, LICENSE AGPL-3.0, DISCLAIMER, main-PCB schematic PDF, 3 packer binaries, nothing else). README confirms STM32F446, app at `0x08010000`, max FW 128 kB, RAM 128 kB, buttons 0+7 update mode, SysEx `F0 00 20 32 00 01 24 00 03 30 F7`, max rate 9.64 kB/s.
- Packer behaviour — **REPRODUCED on this VM (2026-10-28)**: Linux `PRO800_update_gen` on a 1,136-byte test image emitted exactly 7 SysEx messages / 1,473 bytes (header 66 B with `…24 00 0A` + `P0E9I`; data messages 255 B with `…24 00 0B 00`), byte-identical on re-run (deterministic).
- P600fw — **VERIFIED**: `common/` logic/hardware file split matches the roadmap; `.c` files total 7,583 lines; README states GPL v3. Snapshot in `references/p600fw/`.
- REA-Results — **CHECKED (2026-10-28)**: local mirror (Oct 25 snapshot, commit 315e420) grep for pro-800/pro800/p600/stm32f446/behringer = no hits. API routes still 404 (known block, Erik-side collaborator/App-selection fix pending) — noted in one line per standing rule, project continues on local sources.
- Toolchain on this VM — **VERIFIED**: cmake 3.28.3, gcc 13.3.0, `arm-none-eabi-gcc` 13.2.rel1 (the roadmap's linker-script proof used Arm GNU 14.2.rel1 elsewhere — flag if V0 CI shows differences).
- Code — **NOT STARTED**. No repo initialised yet; V0 begins on Garth's go.
- Hardware — **NOT OWNED**. No PRO-800 unit; H-phases gated on purchase + explicit go.

## Decisions made
- Licence: **GPL-3.0-or-later, whole repo** (proposed 2026-10-28, Muse — pending Garth's confirmation; default per his prompt because of P600fw reuse).
- Plugin framework: **JUCE 8 under AGPL-3.0** for V1, with CLAP via clap-juce-extensions (proposed 2026-10-28, Muse — pending confirmation). V0 itself is framework-free (CMake + C99 + CTest) so the core never depends on JUCE. DPF is the documented fallback (see `docs/DECISIONS.md`).

## Open decisions (need the user's call)
1. **Licence/framework confirmation** — GPL-3.0-or-later + JUCE/AGPL as proposed, or DPF/permissive plugin shell instead.
2. **Final product name** — working title `elfchords-p8` only; name is Garth's pick (prompt rule: ask before finalising).
3. **Repo home** — public vs private GitHub repo, and its name (same open question as the LXR-02 plugin had; Garth creates it himself when ready).

## Next actions
- [ ] Garth: confirm (or change) the licence/framework proposal and the first-5-commits plan.
- [ ] Muse: on go — execute commits 1–5 for V0, showing build/test output after each.
