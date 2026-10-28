# p600fw_derived — imported P600fw logic (GPL-3.0)

Verbatim copies from gligli/p600fw `common/` (upstream master, pushed
2026-04-13; snapshot in `references/`), with original copyright headers
intact. Imported logic: adsr, lfo, assigner, arp, seq, utils, clock, midi,
storage, synth, ui, import — plus tuner.c (ADR-004: its CV-compute
functions are logic; its hardware measurement path runs on the bridge's
mocked io and is never exercised on host).

NOT imported (replaced by `core/engine_bridge.c`, which implements their
headers against the P8 HAL): dac.c, display.c, potmux.c, scanner.c, sh.c,
uart_6850.c — the Z80/AVR hardware layer.

xnormidi is imported whole (bytequeue, midi, midi_device, sysex_tools):
P600 midi.c registers its callbacks with the xnormidi device parser and
sends through it, while its SysEx send path drains through uart_send.
bytequeue's AVR interrupt hooks are no-ops in compat.c (single-threaded
core). Correction logged: an early grep missed the midi_register_* calls
in midi_init; the linker found them.

Local changes to upstream code: exactly one, in `patches/
0001-adsr-setgate-div0-guard.patch` (ADR-006) — everything else is
byte-identical to upstream (CRLF line endings preserved).

Portability shims live in `shims/` (print.h, hardware_impl.h,
avr/pgmspace.h) and `compat.c` (itoa). The derived library is built with
`-fgnu89-inline` because utils.c relies on GNU89 inline semantics.
This directory is the isolation boundary from ADR-001: a future
closed-source product must replace everything in here with a clean-room
core.
