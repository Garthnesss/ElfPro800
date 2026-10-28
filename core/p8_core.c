#include "p8_core.h"

void p8_core_init(p8_core_t *core)
{
    core->tick = 0;
}

/* V0 stub tick: engine glue lands with the P600fw import (commit 3).
 * Deterministic: cleared frame, "P8" on the display, heartbeat LED. */
void p8_core_tick(p8_core_t *core, const p8_inputs_t *inputs, p8_cv_frame_t *frame)
{
    (void)inputs;
    p8_frame_clear(frame);
    frame->display[0] = 'P';
    frame->display[1] = '8';
    frame->leds = (core->tick >> 10) & 1u;
    core->tick++;
}
