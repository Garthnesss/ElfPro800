# Unverified hardware assumptions

Every entry stays behind a config constant with a TODO in code until confirmed on a real PRO-800 (H0/H1). Sources: official main-PCB schematic (net names verified as printed) + the 2026-10-28 roadmap; peripheral/AF assignments are inference from STM32F446 docs (RM0390 / DS10693), not fact.

- Peripheral/alternate-function assignment of nearly every pin (UART4 on PA0/PA1, SPI3 for the DAC80501 on PB0/PC10/PA15, SPI4 for the 25M01 EEPROM on PE2/PE5/PE6/PE4, TIM1 encoder on PE9/PE11, I2C1 on PB6/PB7) — inferred.
- HT16K33 I2C address (scan 0x70–0x77 at H0) and its segment/LED/button matrix map — unknown.
- Voice-board schematic and CV scaling — not released; settle via auto-tune measurements.
- Bootloader hand-off state (clocks, IRQs, USB) when the app starts at 0x08010000 — unknown; startup re-inits everything.
- How stock firmware jumps to update mode on the `… 03 30 F7` SysEx — unknown; our firmware replicates it only once the mechanism is known. Recovery until then = buttons 0+7 at power-up (verified in the packer README).
- Whether the bootloader verifies anything beyond the packer's encryption — unknown.
- Stock 25M01 EEPROM layout — unknown; storage is read-only until understood, then we use a separate region.
- Direction of panel-switch nets that also run to the voice board (SAW_A/TRI_A/SAW_B/TRI_B/SYNC/PMOD_*) — confirm per pin before driving any as an output.
- USB connector type on the unit — unconfirmed.
- Whether stock PRO-800 firmware is derived from the public P600fw code — not proven; we assume only shared lineage via the Behringer×GliGli collaboration.
- Community SysEx patch format (samstaton/pro800) beyond what its doc states — import is tolerant by design.
