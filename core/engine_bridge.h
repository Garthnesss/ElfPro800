/* Bridge between the P600fw-derived engine and the P8 HAL.
 *
 * Implements the P600 "firmware or mockup" hardware layer (hardware.h +
 * sh/dac/display/potmux/scanner/uart_6850 headers) in memory: CV and gate
 * writes are captured, pot/button/key state is fed in, MIDI out bytes are
 * buffered, storage is a 56 KB RAM image. This is the mockup the upstream
 * hardware.h anticipates; the PRO-800 target replaces it with real drivers.
 */
#ifndef P8_ENGINE_BRIDGE_H
#define P8_ENGINE_BRIDGE_H

#include <stdint.h>
#include "synth.h" /* p600 enums: p600CV_t, p600Gate_t, p600Pot_t, p600Button_t, p600LED_t */

#define P8_BRIDGE_CV_COUNT 32
#define P8_BRIDGE_MIDI_OUT_MAX 8192

void p8_bridge_reset(void);

/* inputs (target -> engine) */
void p8_bridge_set_pot(p600Pot_t pot, uint16_t value);
void p8_bridge_set_button(p600Button_t button, int8_t pressed);
void p8_bridge_set_key(uint8_t key, int8_t pressed); /* key: 0..60ish, scanner space */

/* outputs (engine -> target) */
uint16_t p8_bridge_cv(p600CV_t cv);
int8_t p8_bridge_gate(p600Gate_t gate);
void p8_bridge_display(char out[5]); /* last sevenSeg content, best effort */
uint32_t p8_bridge_leds(void);       /* bit i = p600LED_t i on */
uint16_t p8_bridge_midi_out_read(uint8_t *buf, uint16_t max);

#endif /* P8_ENGINE_BRIDGE_H */
