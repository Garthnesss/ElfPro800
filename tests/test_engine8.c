/* Patch 0002 proof: an 8-note chord sustains on all 8 voices. */
#include "p8_core.h"
#include "storage.h"
#include "engine_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8_t notes[8] = {48, 55, 60, 64, 67, 72, 76, 79};
    p8_core_t core;
    p8_inputs_t in;
    p8_cv_frame_t f;
    uint8_t on[24], off[24];
    int i, v, distinct = 0;
    for (i = 0; i < 8; i++) {
        on[i * 3] = 0x90; on[i * 3 + 1] = notes[i]; on[i * 3 + 2] = 100;
        off[i * 3] = 0x80; off[i * 3 + 1] = notes[i]; off[i * 3 + 2] = 0;
    }
    memset(&in, 0, sizeof(in));
    p8_core_init(&core);
    preset_loadDefault(1);
    refreshFullState();
    for (i = 0; i < 8000; i++) {
        in.midi_in = NULL; in.midi_in_len = 0;
        if (i == 200) { in.midi_in = on; in.midi_in_len = 24; }
        if (i == 4000) { in.midi_in = off; in.midi_in_len = 24; }
        p8_core_tick(&core, &in, &f);
        if (i == 3000) {
            for (v = 0; v < 8; v++) {
                printf("voice %d: amp %u osc_a %u fil %u\n", v, f.amp[v], f.osc_a[v], f.fil[v]);
                assert(f.amp[v] > 10000); /* every voice sounding */
                if (v && f.osc_a[v] != f.osc_a[0]) distinct++;
            }
            assert(distinct >= 5); /* voices are at different pitches */
        }
    }
    for (v = 0; v < 8; v++)
        assert(f.amp[v] < 2000); /* all released */
    printf("engine8 ok\n");
    return 0;
}
