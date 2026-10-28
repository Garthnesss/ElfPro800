/* Host/ARM shim for <avr/pgmspace.h>: on non-AVR targets PROGMEM data lives
 * in ordinary memory, so the macros collapse to direct access. */
#ifndef P8_SHIM_AVR_PGMSPACE_H
#define P8_SHIM_AVR_PGMSPACE_H
#include <stdint.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
#define pgm_read_float(p) (*(const float *)(p))
#define pgm_read_ptr(p) (*(const void *const *)(p))
#endif
