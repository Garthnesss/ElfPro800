# ElfPro800 plugin (V1 scaffold)

JUCE 8 (AGPL-3.0) plugin wrapping the V0 core: VST3 + Standalone
(AU on macOS). The audio path runs the real engine at 2 kHz; sound comes
from the V0 placeholder voice renderer until the analogue voice model
lands. Parameters wired so far (via the engine's own preset struct +
refreshFullState): Cutoff, Resonance, Filter Env Amt, plus a DSP-side
Volume. The editor is a scaffold (brand + 4 rotaries); the full panel
follows the approved mock.

Build (Linux/macOS/Windows):

    cmake -B build-plugin -S plugin -DCMAKE_BUILD_TYPE=Release
    cmake --build build-plugin

Linux build deps: libasound2-dev, libx11-dev, libxext-dev,
libxinerama-dev, libxrandr-dev, libxcursor-dev, libfreetype-dev,
libfontconfig1-dev, libgl1-mesa-dev (or your distro's equivalents).

Known scaffold simplifications (honest list):
- Placeholder DSP tracks pitch from note on/off (as tools/render.c);
  releasing a middle note can misalign voice slots. The CV-driven voice
  model fixes this properly.
- Engine master-volume pot is not driven (Volume is DSP-side gain).
- SysEx / patch import-export and the 400-preset bank are later V1 steps.
