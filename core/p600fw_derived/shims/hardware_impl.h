/* Shim for P600fw's firmware/hardware_impl.h (AVR atomic/delay macros).
 * The core is single-threaded per tick on host and ARM targets; interrupt
 * masking is the target's job outside the core, so these collapse. */
#ifndef P8_SHIM_HARDWARE_IMPL_H
#define P8_SHIM_HARDWARE_IMPL_H
#define CYCLE_WAIT(cycles) ((void)0)
#define BLOCK_INT
#define MDELAY(ms) ((void)0)
#endif
