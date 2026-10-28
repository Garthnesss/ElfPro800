/* Shim for P600fw's firmware/print.h (which routes to USB debug output).
 * Debug printing is a no-op in the core; targets that want the log can
 * replace this header. */
#ifndef P8_SHIM_PRINT_H
#define P8_SHIM_PRINT_H
#include <avr/pgmspace.h>
static inline void print(const char *s) { (void)s; }
static inline void print_P(const char *s) { (void)s; }
static inline void pchar(int c) { (void)c; }
static inline void phex(unsigned char c) { (void)c; }
static inline void phex16(unsigned int i) { (void)i; }
static inline void phex32(unsigned long i) { (void)i; }
#endif
