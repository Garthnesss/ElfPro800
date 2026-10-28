#include "lfo.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct lfo_s l;
    int i;
    int16_t mn = 32767, mx = -32768;
    lfo_init(&l);
    lfo_setShape(&l, lsTri);
    lfo_setFreq(&l, 40000);
    lfo_setAmt(&l, 65535);
    for (i = 0; i < 200000; i++) {
        int16_t o;
        lfo_update(&l);
        o = lfo_getOutput(&l);
        if (o < mn) mn = o;
        if (o > mx) mx = o;
    }
    assert(mx > 5000 && mn < -5000); /* triangle swings both polarities */
    printf("lfo ok (min %d max %d)\n", mn, mx);
    return 0;
}
