/* Non-standard libc functions P600fw uses (synth.c calls itoa). */
#include <stddef.h>

char *itoa(int value, char *str, int base)
{
    char tmp[16];
    int i = 0, j = 0, neg = 0;
    unsigned int u;
    if (base < 2 || base > 36) { str[0] = '\0'; return str; }
    if (value < 0 && base == 10) { neg = 1; u = (unsigned int)(-(value + 1)) + 1u; }
    else u = (unsigned int)value;
    if (u == 0) tmp[i++] = '0';
    while (u) { int d = (int)(u % (unsigned int)base); tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= (unsigned int)base; }
    if (neg) str[j++] = '-';
    while (i) str[j++] = tmp[--i];
    str[j] = '\0';
    return str;
}

/* xnormidi bytequeue interrupt hooks: the core is single-threaded per
 * tick, so saving/restoring interrupt state is a no-op. */
#include <stdint.h>
typedef uint8_t interrupt_setting_t;
interrupt_setting_t store_and_clear_interrupt(void) { return 0; }
void restore_interrupt_setting(interrupt_setting_t setting) { (void)setting; }

#if defined(_MSC_VER)
/* POSIX random()/srandom() for the MSVC CRT (declared in the print.h
 * shim). Simple LCG; used for LFO random shape and arp randomness, where
 * cross-platform determinism is not required. */
static unsigned long p8_rand_state = 1;
void srandom(unsigned int seed) { p8_rand_state = seed ? seed : 1; }
long random(void)
{
    p8_rand_state = p8_rand_state * 1103515245UL + 12345UL;
    return (long)((p8_rand_state >> 16) & 0x7FFF);
}
#endif
