#include "assigner.h"
#include <assert.h>
#include <stdio.h>

static int count_assigned(void)
{
    int v, n = 0;
    uint8_t note;
    for (v = 0; v < SYNTH_VOICE_COUNT; v++)
        if (assigner_getAssignment(v, &note)) n++;
    return n;
}
static int voice_of(uint8_t want)
{
    int v;
    uint8_t note;
    for (v = 0; v < SYNTH_VOICE_COUNT; v++)
        if (assigner_getAssignment(v, &note) && note == want) return v;
    return -1;
}

int main(void)
{
    uint8_t latest = 0;
    assigner_init();
    assigner_setPriority(apLast);
    assert(!assigner_getAnyAssigned());
    assigner_assignNote(60, 1, 30000, 1);
    assigner_assignNote(64, 1, 30000, 1);
    assigner_assignNote(67, 1, 30000, 1);
    assert(count_assigned() == 3);
    assert(voice_of(60) >= 0 && voice_of(64) >= 0 && voice_of(67) >= 0);
    assert(assigner_getLatestNotePressed(&latest) && latest == 67);
    /* overflow: more notes than voices must not corrupt state */
    assigner_assignNote(69, 1, 30000, 1);
    assigner_assignNote(71, 1, 30000, 1);
    assigner_assignNote(72, 1, 30000, 1);
    assigner_assignNote(74, 1, 30000, 1);
    assert(count_assigned() <= SYNTH_VOICE_COUNT);
    assigner_allKeysOff();
    assert(!assigner_getAnyPressed());
    printf("assigner ok\n");
    return 0;
}
