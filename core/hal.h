/* ElfChords P8 - core/target HAL contract (see ARCHITECTURE.md).
 *
 * The core consumes p8_inputs_t and produces one p8_cv_frame_t per control
 * tick. The plugin target turns frames into audio; the PRO-800 firmware
 * target turns them into DAC/S&H writes. GPL-3.0-or-later.
 */
#ifndef P8_HAL_H
#define P8_HAL_H

#include <stdint.h>

#define P8_VOICE_COUNT 8
#define P8_POT_COUNT 26
#define P8_CONTROL_RATE_HZ 2000

typedef struct {
    uint16_t pots[P8_POT_COUNT]; /* 0..65535 */
    uint32_t switches;           /* panel switch bitfield (map: targets) */
    uint32_t buttons;            /* panel button bitfield (map: targets) */
    int16_t encoder_delta;       /* VALUE encoder movement since last tick */
    const uint8_t *midi_in;      /* MIDI byte stream chunk, may be NULL */
    uint16_t midi_in_len;
    uint32_t sync_ticks;         /* external sync pulses since last tick */
    uint16_t tuner_capture;      /* tuner capture value; UNVERIFIED on hw */
} p8_inputs_t;

typedef struct {
    uint16_t osc_a[P8_VOICE_COUNT];
    uint16_t osc_b[P8_VOICE_COUNT];
    uint16_t fil[P8_VOICE_COUNT];
    uint16_t amp[P8_VOICE_COUNT];
    uint16_t pw_a, pw_b, vol_a, vol_b, res, noise, mvol, pmod_b, ext_fil;
    uint32_t voice_bits; /* waveform/sync/polymod/unison/keytrack bits */
    char display[5];     /* 4 chars + NUL */
    uint32_t leds;
    uint8_t midi_out[64];
    uint16_t midi_out_len;
} p8_cv_frame_t;

void p8_frame_clear(p8_cv_frame_t *frame);

#endif /* P8_HAL_H */
