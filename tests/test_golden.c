/* Golden CvFrame snapshot: a fixed MIDI scenario must produce these exact
 * frame values. If an engine change is intentional, update the constants
 * in the same commit and say why. Scenario: init patch, C major triad
 * (60/64/67) on at tick 200, off at tick 4000, 8000 ticks total. */
#include "p8_core.h"
#include "storage.h"
#include "engine_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { int tick; uint16_t amp0, amp1, amp2, osc0, fil0, mvol; } snap_t;
static const snap_t GOLD[] = {
    /* tick, amp0, amp1, amp2, osc_a0, fil0, mvol */
    {1000, 59391, 59391, 59391, 17039, 51610, 0},
    {3000, 59391, 59391, 59391, 17039, 51610, 0},
    {5000, 0, 0, 0, 17039, 51610, 0},
    {7999, 0, 0, 0, 17039, 51610, 0},
};

int main(void)
{
    p8_core_t core;
    p8_inputs_t in;
    p8_cv_frame_t f;
    uint8_t on[9] = {0x90, 60, 100, 0x90, 64, 100, 0x90, 67, 100};
    uint8_t off[9] = {0x80, 60, 0, 0x80, 64, 0, 0x80, 67, 0};
    int i, gi = 0;
    memset(&in, 0, sizeof(in));
    p8_core_init(&core);
    preset_loadDefault(1);
    refreshFullState();
    for (i = 0; i < 8000; i++) {
        in.midi_in = NULL; in.midi_in_len = 0;
        if (i == 200) { in.midi_in = on; in.midi_in_len = 9; }
        if (i == 4000) { in.midi_in = off; in.midi_in_len = 9; }
        p8_core_tick(&core, &in, &f);
        if (gi < 4 && i == GOLD[gi].tick) {
            printf("tick %d: amp %u %u %u osc %u fil %u mvol %u\n", i, f.amp[0], f.amp[1], f.amp[2], f.osc_a[0], f.fil[0], f.mvol);
#ifdef P8_GOLDEN_GENERATE
            printf("    {%d, %u, %u, %u, %u, %u, %u},\n", i, f.amp[0], f.amp[1], f.amp[2], f.osc_a[0], f.fil[0], f.mvol);
#else
            assert(f.amp[0] == GOLD[gi].amp0 && f.amp[1] == GOLD[gi].amp1 && f.amp[2] == GOLD[gi].amp2);
            assert(f.osc_a[0] == GOLD[gi].osc0 && f.fil[0] == GOLD[gi].fil0 && f.mvol == GOLD[gi].mvol);
#endif
            gi++;
        }
    }
    printf("golden ok\n");
    return 0;
}
