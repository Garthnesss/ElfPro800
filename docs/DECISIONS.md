# Decisions (ADR-style)

## ADR-001 — Licence: GPL-3.0-or-later for the whole repo (2026-10-28, proposed by Muse, pending Garth's confirmation)
We reuse GliGli's P600fw logic (GPL-3.0), so anything containing it must be GPL-3.0 (AGPL-3.0 is combinable via §13 of both licences). Default per Garth's prompt. Consequence: binaries may be sold, but source must be offered. P600fw-derived code is isolated in `core/p600fw_derived/` so a closed-source product remains possible later *only* via a clean-room core swap.

## ADR-002 — Plugin framework: JUCE 8 under AGPL-3.0, from V1 (2026-10-28, proposed, pending confirmation)
JUCE under AGPL-3.0 is licence-compatible with the GPL-3.0 core. Chosen over DPF/iPlug2 because: AU + VST3 + standalone on macOS is the most battle-tested path (Garth's DAW platform is Live on Mac; his LXR-02 plugin plan and the Behringer KICK codebase are also JUCE), `pluginval` + Steinberg validator are the V1 acceptance gates and are JUCE-native in practice, and CLAP is covered by the mature clap-juce-extensions (MIT). VST3 SDK ≥3.8 is MIT.
**V0 uses no plugin framework at all** (CMake + C99 + CTest) — the core must never depend on JUCE.
Fallback: DPF (ISC) if AGPL terms ever bite — its AU support is newer/weaker, which is why it is not the first pick.

## ADR-003 — Upstream references are snapshots, not code (2026-10-28, Muse)
`references/` holds read-only snapshots of PRO800-Firmware-Packer @ `c845054` and p600fw (upstream pushed 2026-04-13) for offline reference. The packer binary is used as a tool only; its output encryption is never re-implemented.
