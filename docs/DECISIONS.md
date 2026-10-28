# Decisions (ADR-style)

## ADR-001 — Licence: GPL-3.0-or-later for the whole repo (2026-10-28, proposed by Muse, CONFIRMED by Garth same day)
We reuse GliGli's P600fw logic (GPL-3.0), so anything containing it must be GPL-3.0 (AGPL-3.0 is combinable via §13 of both licences). Default per Garth's prompt. Consequence: binaries may be sold, but source must be offered. P600fw-derived code is isolated in `core/p600fw_derived/` so a closed-source product remains possible later *only* via a clean-room core swap.

## ADR-002 — Plugin framework: JUCE 8 under AGPL-3.0, from V1 (2026-10-28, proposed, CONFIRMED by Garth same day)
JUCE under AGPL-3.0 is licence-compatible with the GPL-3.0 core. Chosen over DPF/iPlug2 because: AU + VST3 + standalone on macOS is the most battle-tested path (Garth's DAW platform is Live on Mac; his LXR-02 plugin plan and the Behringer KICK codebase are also JUCE), `pluginval` + Steinberg validator are the V1 acceptance gates and are JUCE-native in practice, and CLAP is covered by the mature clap-juce-extensions (MIT). VST3 SDK ≥3.8 is MIT.
**V0 uses no plugin framework at all** (CMake + C99 + CTest) — the core must never depend on JUCE.
Fallback: DPF (ISC) if AGPL terms ever bite — its AU support is newer/weaker, which is why it is not the first pick.

## ADR-003 — Upstream references are snapshots, not code (2026-10-28, Muse)
`references/` holds read-only snapshots of PRO800-Firmware-Packer @ `c845054` and p600fw (upstream pushed 2026-04-13) for offline reference. The packer binary is used as a tool only; its output encryption is never re-implemented.

## ADR-004 — tuner.c is imported with the logic files (2026-10-28, Muse)
The roadmap classifies tuner.c as hardware-coupled, and its measurement path is — but `tuner_computeCVFromNote` is pure logic that synth.c needs for every pitch CV. Importing it whole (measurement path running on the bridge's mocked io, never exercised on host) beats reimplementing its tuning math. The PRO-800 target still rewrites the measurement path in H1.

## ADR-005 — Engine rate: 500 Hz inside a 2 kHz frame rate (2026-10-28, Muse)
P600fw's timer interrupt runs at 500 Hz (TICKER_1S=500). `p8_core_tick` runs at the HAL's 2 kHz and advances the engine timer every 4th tick; `synth_update` (panel housekeeping) runs every tick. Envelopes/LFO therefore keep hardware timing while targets get 2 kHz frames.

## ADR-006 — One carried patch: ADSR divide-by-zero guard (2026-10-28, Muse)
`adsr_setGate` divides by `levelCV`, which is 0 until the first note-on sets it (synth_assignerEvent sets gate *before* the velocity/level block). AVR and Cortex-M UDIV-by-zero don't trap, so upstream never sees it; x86 raises SIGFPE — found by running a MIDI note-on probe on this VM (UBSan: adsr.c:161 division by zero). Fix is a one-line guard in `patches/0001-adsr-setgate-div0-guard.patch`, behaviour-identical in the only reachable case (output is 0 whenever levelCV is 0 at gate time). Everything else in p600fw_derived stays byte-identical to upstream.

## ADR-007 — 8-voice expansion by carried patch (2026-10-28, Garth's call)

- Decision: expand the imported P600 engine from 6 to 8 voices as carried
  patch `core/p600fw_derived/patches/0002-eight-voices.patch`, instead of
  running two 4-voice instances or deferring to the plugin stage.
- What it touches: `SYNTH_VOICE_COUNT` 6→8; the CV enum gains pcOsc7A/8A,
  pcOsc7B/8B, pcFil7/8, pcAmp7/8 and the global CVs shift 24→32 (bridge CV
  count 32→40); `pcFil6`/`pcOsc6B` "last CV" references become
  `pcFil8`/`pcOsc8B` (incl. `TUNER_CV_COUNT` 24→32); the `%6` detune-spread
  rotations become `%SYNTH_VOICE_COUNT`; the six unrolled `refreshVoice`
  calls upstream labelled "SYNTH_VOICE_COUNT calls" become a loop; default
  voiceMask `0x3f` becomes `(1<<SYNTH_VOICE_COUNT)-1` in assigner+storage.
- Everything else scaled by the define already (63 uses across 8 files) —
  assigner, tuner, storage loops and voicePattern needed no edits.
- Consequence: our settings/preset storage layout diverges from stock P600
  from this commit on (tunes table 32 CVs, voicePattern 8 entries). That is
  intended: the PRO-800 target is an 8-voice machine and the plugin defines
  its own bank format in V1. Stock-P600 SysEx patch import, if ever wanted,
  is a converter, not the native format.
- Regression proof: the V0 golden snapshot (voices 0–2, spread 0) passes
  **unchanged** after the patch; new engine8 test holds an 8-note chord
  with all 8 amp CVs sustaining at distinct pitches.
