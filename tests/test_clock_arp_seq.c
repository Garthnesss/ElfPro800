#include "clock.h"
#include "arp.h"
#include "seq.h"
#include "midi.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int i, pulses;
    uint16_t period;

    /* clock: stopped below CV 1024 */
    clock_reset();
    clock_setSpeed(0);
    assert(clock_getSpeed() == UINT16_MAX);
    pulses = 0;
    for (i = 0; i < 10000; i++) pulses += clock_update();
    assert(pulses == 0);

    /* clock: pulses exactly every `period` updates at internal sync */
    clock_reset();
    clock_setSpeed(65535);
    period = clock_getSpeed();
    assert(period != UINT16_MAX && period > 0);
    pulses = 0;
    for (i = 0; i < (int)period * 10; i++) pulses += clock_update();
    assert(pulses == 10);
    printf("clock ok (period %u)\n", period);

    /* arp: mode/hold state machine (musical stepping is engine-level, V1).
     * midi_init first: arp_update emits through midi_sendNoteEvent, whose
     * xnormidi send hook is registered by midi_init (null-pointer crash
     * without it - found by ASan, midi.c:540 via arp.c:280). */
    midi_init();
    arp_init();
    assert(arp_getMode() == amOff);
    arp_setMode(amUpDown, 1);
    assert(arp_getMode() == amUpDown && arp_getHold() == 1);
    arp_assignNote(60, 1);
    arp_assignNote(64, 1);
    for (i = 0; i < 100000; i++) arp_update();
    arp_setMode(amOff, 0);
    assert(arp_getMode() == amOff);

    /* seq: mode state + step count sanity */
    seq_init();
    assert(seq_getMode(0) == smOff);
    seq_setMode(0, smPlaying);
    assert(seq_getMode(0) == smPlaying);
    assert(seq_getStepCount(0) <= 64);
    assert(!seq_full(0));
    seq_setMode(0, smOff);
    printf("arp/seq ok\n");
    return 0;
}
