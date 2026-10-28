# ElfChords P8 — Status

**Last updated:** 2026-10-28
**Phase:** V0 complete on this VM (with two flagged deviations below). Next: V1 plugin, or the 8-voice expansion first — Garth's call.

## Where things stand
- V0 commits 1–5 — **DONE + VERIFIED on this VM (2026-10-28)**: local git repo at `~/workspace/elfchords-p8`, commits `e190863` (scaffold), `0e1e46c` (CMake/HAL/CI), `c146083` (P600fw import + bridge), `dcba0e5` (unit tests), `07591f9` (render + golden). Not pushed anywhere — no remote repo exists yet.
- Tests — **ctest 9/9 pass** on Linux: smoke, adsr (incl. patch-0001 regression), lfo, assigner, clock/arp/seq, engine (note on/off, sustain, running status, determinism), golden CvFrame snapshot, render smoke + WAV verify.
- Render — **WORKS**: `tools/render tests/data/chord.mid out` → CSV of every CvFrame + 48 kHz WAV of a Cmaj chord (peak 28409, RMS 8317, sustained while held, released at note-off). Placeholder DSP only (pitch from MIDI; engine amp/filter CVs per voice) — the analog voice model is V1.
- ARM cross-compile — **GREEN**: core + derived engine build for Cortex-M4 hard-float; combined text 52,227 bytes (fits the 128 kB PRO-800 app budget with room for the H-phase USB/HAL).
- Upstream import — byte-identical to gligli/p600fw except **one** carried patch (ADR-006: ADSR divide-by-zero guard, `patches/0001`). Findings along the way, all reproduced on this VM: the div-by-zero itself (UBSan, adsr.c:161); notes gated during the engine's post-init preset load are dropped (~100 frames boot guard in render); blank storage boots an all-zero preset (2 ms envelope blips) — init patch = `preset_loadDefault` + `refreshFullState()`.

## Deviations from the V0 gate in the source prompt (need Garth's call)
1. **Voices: 6, not 8.** P600fw is a 6-voice engine and its CV enum/preset structures are 6-fixed — expanding to 8 is real surgery on assigner/synth/storage, not a define flip. The P8 CvFrame is already 8-wide (voices 7–8 silent). Recommendation: make 8-voice expansion the first V1 milestone, before the plugin DSP voices multiply the problem.
2. **CI on 3 OSes: unverified.** The GitHub Actions workflow is committed, but there is no remote repo yet, so only the Linux host build is proven. macOS/Windows green awaits the repo (open decision 3).

## Decisions made
- Licence: **GPL-3.0-or-later, whole repo** (ADR-001 — confirmed by Garth 2026-10-28).
- Plugin framework: **JUCE 8 under AGPL-3.0** for V1, CLAP via clap-juce-extensions; DPF fallback (ADR-002 — confirmed by Garth 2026-10-28).
- ADR-003 upstream snapshots; ADR-004 tuner.c imported whole; ADR-005 engine runs 500 Hz inside the 2 kHz frame rate; ADR-006 one carried patch (ADSR div0 guard).

## Open decisions (need the user's call)
1. **Next step order** — 8-voice expansion first (recommended), or straight into the V1 JUCE plugin with 6 voices and expand later.
2. **Final product name** — working title `elfchords-p8` only.
3. **Repo home** — public vs private GitHub repo + name (Garth creates it; then CI can verify macOS/Windows and the work can be pushed).

## Next actions
- [ ] Garth: pick the next-step order; create the GitHub repo when ready and hand over the URL.
- [ ] Muse: on go — start the chosen milestone; when a remote exists, push commits 1–5 and confirm CI on all 3 OSes.
## Update 2026-10-28 (later) — 8-voice expansion DONE + repo created

Garth chose "8-voice expansion first, then V1" and created the repo:
**https://github.com/Garthnesss/ElfPro800** (public). Expansion landed as
carried patch 0002 (ADR-007): SYNTH_VOICE_COUNT 8, CV enum extended
(globals shift to 32, bridge CV count 40), %6 spread rotations and
refreshVoice unroll generalized, voiceMask defaults to all 8 voices.
Evidence: ctest **10/10** (new engine8 test: 8-note chord sustains on all
8 amp CVs at distinct pitches, all release; assigner fills exactly 8);
the V0 golden snapshot passes **unchanged**; ARM text 52,149 B (loop
replaced unrolled calls — slightly smaller than the 6-voice build).
8-note render proof: `elfchords-p8-8voice-chord.wav` (peak 18144;
demo mixer gain lowered 0.25→0.11 for 8-voice headroom). Next: V1 JUCE
plugin (6→8 voices already native now).

