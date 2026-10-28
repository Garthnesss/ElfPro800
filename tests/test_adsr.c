#include "adsr.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct adsr_s a;
    int i;
    adsr_init(&a);
    /* fast A/D/R, sustain at half, full level */
    adsr_setCVs(&a, 65535, 65535, 32768, 65535, 65535, 0x1f);
    adsr_setShape(&a, 0);
    adsr_setGate(&a, 1); /* would SIGFPE pre-patch-0001 on first gate: levelCV set above, so also exercise the patched path below */
    for (i = 0; i < 500000 && adsr_getStage(&a) != sSustain; i++)
        adsr_update(&a);
    assert(adsr_getStage(&a) == sSustain);
    {
        uint16_t out = adsr_getOutput(&a);
        assert(out > 20000 && out < 45000); /* ~sustain 32768 */
    }
    adsr_setGate(&a, 0);
    assert(adsr_getStage(&a) == sRelease);
    for (i = 0; i < 500000 && adsr_getOutput(&a) > 500; i++)
        adsr_update(&a);
    assert(adsr_getOutput(&a) <= 500);

    /* patch 0001 regression: gate with levelCV never set must not divide by zero */
    adsr_init(&a);
    adsr_setCVs(&a, 65535, 65535, 32768, 65535, 0, 0x0f); /* level bit (0x10) clear, like synth.c periodic refresh */
    adsr_setGate(&a, 1);
    adsr_setGate(&a, 0);
    printf("adsr ok\n");
    return 0;
}
