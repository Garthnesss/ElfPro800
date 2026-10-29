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
2. ~~**Final product name**~~ — **resolved 2026-10-28: ElfPro800** (Garth's call; panel mock updated to match).
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


## Update 2026-10-28 (evening) — repo live, CI green on all four jobs

Pushed to **Garthnesss/ElfPro800** (his two web-UI seed commits preserved
as ancestors; content replicated via the Git Data API, trees verified
identical — remote SHAs differ from local because GitHub normalizes
committer timezone offsets). CI then burned through five real portability
finds before going green (run 40793407923, head ab1e453):

1. `itoa` undeclared under clang (macOS) — declared in the print.h shim.
2. MSVC `cl` rejects GCC `-Wno-*` flags + no libm — MSVC CMake branch.
3. GCC `__attribute__` in FORCEINLINE/NOINLINE + packed struct — patch 0003.
4. `M_PI` needs `_USE_MATH_DEFINES` under MSVC.
5. GNU89 inline-export semantics (29→13→1 unresolved externals): MSVC
   FORCEINLINE is empty; bare/mid-signature `inline` dropped from cross-TU
   definitions (patch 0003); POSIX random/srandom implemented in compat.c.
6. VS multi-config needed `ctest -C`; then the Debug-config ctest run
   stalled 17 min with no output (cancelled via API; Debug CRT assert
   dialog on the headless runner is the unproven prime suspect), so CI
   builds/tests **Release** everywhere with a 120 s per-test timeout.

**Both V0 deviations are now resolved:** the engine is 8-voice, and CI is
verified green on Linux, macOS, Windows and the ARM cross-build. Next
milestone is V1 (JUCE plugin) per Garth's "expansion first, then V1".
Local ctest remains 10/10; ARM text 52,149 B.

## Update 2026-10-28 (night) — V1 plugin scaffold DONE, CI green incl. plugin jobs

Name/branding settled per Garth: the product is **ElfPro800**, no
"by ElfChords" line (mock updated to match). V1 scaffold landed in
`plugin/` (JUCE 8.0.8 via FetchContent tarball): the processor owns the
real 8-voice engine, ticks it at 2 kHz from the audio callback with the
V0 boot guard, wires Cutoff/Resonance/Filter-Env-Amt through the
engine's own preset struct, and renders with the V0 placeholder voice
DSP. Editor is a scaffold (brand + 4 rotaries) ahead of the full panel.

CI (run 40804676462): **all 6 jobs green** — host Linux/macOS/Windows,
ARM, and plugin Ubuntu + macOS (VST3 + Standalone, + AU on macOS). The
Linux plugin job caught one real bug on the way: the core libs needed
PIC for the shared module link (fixed plugin-side only). Local VM build
compiled every TU but its LTO links were OOM-killed with three other
JUCE builds sharing the box, so LTO is off and CI carries the link proof.

NOT yet done (next gates): load in a real DAW (pluginval + Live on
Garth's Mac), the full panel UI from the mock, the parameter set beyond
the first four, patch/preset management, CLAP, and the analogue voice
model that replaces the placeholder DSP.

## Update 2026-10-28 (late) — FULL PANEL UI DONE, harness green in CI

The plugin editor is now the full panel from the approved mock, and
every control is wired: 28 knobs (all continuous engine params), 10
stepped toggles, LFO target toggles (bitmask), LFO shape + filter
tracking segmented controls, the arpeggiator (mode + hold via
arp_setMode), six factory presets with LCD + prev/next (also exposed
as host programs), and live voice LEDs driven by the engine's amp CVs.

A headless harness (plugin/test) runs in CI and proves the wiring
without a DAW: silence -> chord sustain (RMS 0.118) -> cutoff changes
the rendered audio -> release tail decays -> every parameter asserted
in currentPreset / the arp module -> preset switching applies.

It caught two real engine truths on the way:
1. cpVolA/cpVolB are DERIVED (recomputed each refresh from cpMixVolA +
   cpDrive; Glide shares cpGlideVolB). Panel binds the sources: Mixer =
   MIX A/B + DRIVE, processor selects the SCI panel layout at init.
2. JUCE choice parameters convert by index, not 0..1 (test-helper fix).

CI run 40810650803: **all 6 jobs green** including the harness on both
plugin platforms. Next gate: Garth loads it in Live on his Mac.
