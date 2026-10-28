#include "p8_core.h"
#include "engine_bridge.h"
#include "synth.h"
#include <string.h>

/* The P600fw-derived engine runs at a 500 Hz timer rate (TICKER_1S=500);
 * the P8 frame rate is 2 kHz, so the engine timer advances every 4th tick
 * while synth_update (panel/UI housekeeping) runs every tick. ADR-005. */
#define P8_ENGINE_DIVIDER (P8_CONTROL_RATE_HZ / 500)

void p8_core_init(p8_core_t *core)
{
    core->tick = 0;
    p8_bridge_reset();
    synth_init();
}

void p8_core_tick(p8_core_t *core, const p8_inputs_t *inputs, p8_cv_frame_t *frame)
{
    uint16_t i;
    int v;

    if (inputs && inputs->midi_in) {
        for (i = 0; i < inputs->midi_in_len; i++)
            synth_uartEvent(inputs->midi_in[i]);
    }

    if ((core->tick % P8_ENGINE_DIVIDER) == 0)
        synth_timerInterrupt();
    synth_update();

    p8_frame_clear(frame);
    for (v = 0; v < P8_VOICE_COUNT; v++) {
        if (v < SYNTH_VOICE_COUNT) {
            frame->osc_a[v] = p8_bridge_cv((p600CV_t)(pcOsc1A + v));
            frame->osc_b[v] = p8_bridge_cv((p600CV_t)(pcOsc1B + v));
            frame->fil[v] = p8_bridge_cv((p600CV_t)(pcFil1 + v));
            frame->amp[v] = p8_bridge_cv((p600CV_t)(pcAmp1 + v));
        }
    }
    frame->pw_a = p8_bridge_cv(pcAPW);
    frame->pw_b = p8_bridge_cv(pcBPW);
    frame->vol_a = p8_bridge_cv(pcVolA);
    frame->vol_b = p8_bridge_cv(pcVolB);
    frame->res = p8_bridge_cv(pcResonance);
    frame->mvol = p8_bridge_cv(pcMVol);
    frame->pmod_b = p8_bridge_cv(pcPModOscB);
    frame->ext_fil = p8_bridge_cv(pcExtFil);
    /* noise: the P600 engine has no noise CV; stays 0 in V0. */
    frame->voice_bits = (uint32_t)(p8_bridge_gate(pgASaw) |
                        (p8_bridge_gate(pgATri) << 1) |
                        (p8_bridge_gate(pgSync) << 2) |
                        (p8_bridge_gate(pgBSaw) << 3) |
                        (p8_bridge_gate(pgBTri) << 4) |
                        (p8_bridge_gate(pgPModFA) << 5) |
                        (p8_bridge_gate(pgPModFil) << 6));
    p8_bridge_display(frame->display);
    frame->leds = p8_bridge_leds();
    frame->midi_out_len = p8_bridge_midi_out_read(frame->midi_out, sizeof(frame->midi_out));

    core->tick++;
}
