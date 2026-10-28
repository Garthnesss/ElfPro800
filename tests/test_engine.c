/* Engine-level tests through the P8 HAL: MIDI in -> CvFrames out. */
#include "p8_core.h"
#include "storage.h"
#include "engine_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t run_scenario(int running_status)
{
    p8_core_t core;
    p8_inputs_t in;
    p8_cv_frame_t f;
    uint32_t hash = 2166136261u;
    uint16_t maxamp0 = 0, maxamp1 = 0, held_amp0 = 0;
    int i;
    uint8_t on[3] = {0x90, 60, 100};
    uint8_t on2[2] = {64, 100}; /* running-status note on */
    uint8_t off[3] = {0x80, 60, 0};

    memset(&in, 0, sizeof(in));
    p8_core_init(&core);
    preset_loadDefault(1); /* init patch: full sustain (see render.c) */
    refreshFullState();
    for (i = 0; i < 8000; i++) {
        in.midi_in = NULL; in.midi_in_len = 0;
        if (i == 100) { in.midi_in = on; in.midi_in_len = 3; }
        if (running_status && i == 200) { in.midi_in = on2; in.midi_in_len = 2; }
        if (i == 4000) { in.midi_in = off; in.midi_in_len = 3; }
        p8_core_tick(&core, &in, &f);
        if (f.amp[0] > maxamp0) maxamp0 = f.amp[0];
        if (i == 3000) held_amp0 = f.amp[0];
        if (f.amp[1] > maxamp1) maxamp1 = f.amp[1];
        hash = (hash ^ f.amp[0]) * 16777619u;
        hash = (hash ^ f.osc_a[0]) * 16777619u;
        hash = (hash ^ (uint32_t)i) * 16777619u;
    }
    printf("scenario rs=%d maxamp0=%u maxamp1=%u held_amp0=%u final_amp0=%u hash=%08x\n",
           running_status, maxamp0, maxamp1, held_amp0, f.amp[0], hash);
    assert(maxamp0 > 10000);                    /* note sounded */
    assert(held_amp0 > 10000);                  /* and sustained while held */
    if (running_status) assert(maxamp1 > 10000); /* running status parsed, 2nd voice */
    assert(f.amp[0] < maxamp0 / 2);              /* release fell after note off */
    return hash;
}

int main(void)
{
    uint32_t h1 = run_scenario(0);
    uint32_t h2 = run_scenario(0);
    assert(h1 == h2); /* deterministic */
    (void)run_scenario(1);
    printf("engine ok\n");
    return 0;
}
